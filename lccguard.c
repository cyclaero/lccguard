//  lccguard.c
//
//  Created by Dr. Rolf Jansen on 2016-08-24.
//  Copyright 2016 Dr. Rolf Jansen. All rights reserved.
//
// sudo clang lccguard.c -Wno-empty-body -Wno-parentheses -O3 -g0 -o /usr/local/bin/lccguard
// sudo strip /usr/local/bin/lccguard


#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>


#define DAEMON_NAME "lccguard"

const char  *pidfname = "/var/run/lccguard.pid";
const char **fdummies = NULL;


void usage(const char *executable)
{
   const char *r = executable + strlen(executable);
   while (--r >= executable && *r != '/'); r++;
   printf("\nusage: %s [-p file] [-f] [-n] [-t] [-h] dummy_file_0 [dummy_file_1] ...\n", r);
   printf(" -p file    the path to the pid file [default: /var/run/lccguard.pid]\n");
   printf(" -f         foreground mode, don't fork off as a daemon.\n");
   printf(" -n         no console, don't fork off as a daemon - started/managed by launchd.\n");
   printf(" -t         idle time in seconds, [default: 4 s].\n");
   printf(" -h         shows these usage instructions.\n");
   printf(" dummy_file_0 [dummy_file_1] [dummy_file_2] ...\n");
   printf("            the full path names of the dummy files to be repeatedly re-written.\n\n");
}


void cleanup(void)
{
   for (int k = 0; fdummies[k]; k++)
   {
      unlink(fdummies[k]);
      free((void *)fdummies[k]);
   }
   free(fdummies);
   unlink(pidfname);
}


static void signals(int sig)
{
   switch (sig)
   {
      case SIGHUP:
         syslog(LOG_ERR, "Received SIGHUP signal.");
         kill(0, SIGHUP);
         exit(0);
         break;

      case SIGINT:
         syslog(LOG_ERR, "Received SIGINT signal.");
         kill(0, SIGINT);
         exit(0);
         break;

      case SIGQUIT:
         syslog(LOG_ERR, "Received SIGQUIT signal.");
         kill(0, SIGQUIT);
         exit(0);
         break;

      case SIGTERM:
         syslog(LOG_ERR, "Received SIGTERM signal.");
         kill(0, SIGTERM);
         exit(0);
         break;

      default:
         syslog(LOG_ERR, "Unhandled signal (%d) %s", sig, strsignal(sig));
         break;
   }
}


typedef enum
{
   noDaemon,
   launchdDaemon,
   discreteDaemon
} DaemonKind;


void daemonize(DaemonKind kind)
{
   switch (kind)
   {
      case noDaemon:
         signal(SIGINT, signals);
         openlog(DAEMON_NAME, LOG_NDELAY | LOG_PID | LOG_CONS, LOG_USER);
         break;

      case launchdDaemon:
         signal(SIGTERM, signals);
         openlog(DAEMON_NAME, LOG_NDELAY | LOG_PID, LOG_USER);
         break;

      case discreteDaemon:
      {
         // fork off the parent process
         pid_t pid = fork();

         if (pid < 0)
            exit(EXIT_FAILURE);

         // if we got a good PID, then we can exit the parent process.
         if (pid > 0)
            exit(EXIT_SUCCESS);

         // The child process continues here.
         // first close all open descriptors
         for (int i = getdtablesize(); i >= 0; --i)
            close(i);

         // re-open stdin, stdout, stderr connected to /dev/null
         int inouterr = open("/dev/null", O_RDWR);    // stdin
         dup(inouterr);                               // stdout
         dup(inouterr);                               // stderr

         // Change the file mode mask, 027 = complement of 750
         umask(027);

         pid_t sid = setsid();
         if (sid < 0)
            exit(EXIT_FAILURE);     // should log the failure before exiting?

         // Check and write our pid lock file
         // and mutually exclude other instances from running
         int pidfile = open(pidfname, O_RDWR|O_CREAT, 0640);
         if (pidfile < 0)
            exit(1);                // can not open our pid file

         if (lockf(pidfile, F_TLOCK, 0) < 0)
            exit(0);                // can not lock our pid file -- was locked already

         // only first instance continues beyound this
         char s[256];
         int  l = snprintf(s, 256, "%d\n", getpid());
         write(pidfile, s, l);      // record pid to our pid file

         signal(SIGHUP,  signals);
         signal(SIGINT,  signals);
         signal(SIGQUIT, signals);
         signal(SIGTERM, signals);
         signal(SIGCHLD, SIG_IGN);  // ignore child
         signal(SIGTSTP, SIG_IGN);  // ignore tty signals
         signal(SIGTTOU, SIG_IGN);
         signal(SIGTTIN, SIG_IGN);

         openlog(DAEMON_NAME, LOG_NDELAY | LOG_PID, LOG_USER);
         break;
      }
   }
}


int main(int argc, char *argv[])
{
   char        ch, *p;
   int         i, k, fd;
   int64_t     idle  = 4;
   const char *cmd   = argv[0];
   DaemonKind  dKind = discreteDaemon;

   while ((ch = getopt(argc, argv, "p:fnt:h")) != -1)
   {
      switch (ch)
      {
         case 'p':
            pidfname = optarg;
            break;

         case 'f':
            dKind = noDaemon;
            break;

         case 'n':
            dKind = launchdDaemon;
            break;

         case 't':
            if ((idle = strtol(optarg, &p, 10)) <= 0)
            {
               usage(cmd);
               return 1;
            }
            break;

         case 'h':
            usage(cmd);
            return 0;

         default:
            usage(cmd);
            return 1;
            break;
      }
   }

   argc -= optind;
   argv += optind;
   if (argc == 0)
   {
      usage(cmd);
      return 1;
   }

   daemonize(dKind);

   if (fdummies = calloc(argc+1, sizeof(const char *)))
   {
      for (i = 0, k = 0; i < argc; i++)
      {
         if (p = malloc(strlen(argv[i])+1))
         {
            if ((fd = open(argv[i], O_WRONLY|O_CREAT|O_TRUNC|O_EXLOCK, 0666)) != -1)
            {
               fdummies[k++] = strcpy(p, argv[i]);
               fcntl(fd, F_NOCACHE, 1);
               write(fd, "load cycle prevention\n", 22);
               close(fd);
            }
            else
               free(p);
         }
         else
         {
            cleanup();
            exit(1);
         }
      }

      if (!k)
      {
         free(fdummies);
         exit(1);
      }
      else
      {
         atexit(cleanup);

         for (;;)
         {
            sleep(idle);
            for (k = 0; fdummies[k]; k++)
            {
               if ((fd = open(fdummies[k], O_WRONLY|O_CREAT|O_TRUNC|O_EXLOCK, 0666)) != -1)
               {
                  fcntl(fd, F_NOCACHE, 1);
                  write(fd, "load cycle prevention\n", 22);
                  close(fd);
               }
            }
         }
      }
   }

   return 0;
}
