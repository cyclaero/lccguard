**A small daemon for stopping Load Cycling of the startup disk of Mac OS X systems**

Some Mac hard disks do not respond to [hdapm - A Mac utility for setting the power management (APM) level on SATA hard drives](http://mckinlay.net.nz/hdapm/), and in this case the only possibility to prevent excessive load cycling of the startup disk of a Mac and by this [beating it to death](http://www.kg4cyx.net/mac-os-x-is-beating-your-hard-drives-to-death-heres-the-fix/), is to frequently write-flush
a small amount of data to it.

[**lccguard**](https://obsigna.net/?p=647) is a small standalone daemon, that writes a tiny string to /var/tmp/lccguard.dummy every 4 seconds, and by this way prevents the respective disk from load cycling its heads. The frequency can be adjusted:

    $ lccguard -h
    usage: lccguard [-p file] [-f] [-n] [-h]
     -p file    the path to the pid file [default: /var/run/lccguard.pid]
     -f         foreground mode, don't fork off as a daemon.
     -n         no console, don't fork off as a daemon - started/managed by launchd.
     -t         idle time in seconds, [default: 4 s].
     -h         shows these usage instructions.


**Installation and First Start**

1. Compile lccguard.c

         sudo mkdir -p /usr/local/bin [optional, only if /usr/local/bin does not exist]
         sudo clang lccguard.c -Wno-empty-body -Ofast -g0 -o /usr/local/bin/lccguard
         sudo strip /usr/local/bin/lccguard
   
2. Place the lccguard.plist file int /Library/LaunchDaemons/lccguard.plist

         sudo cp lccguard.plist /Library/LaunchDaemons/lccguard.plist

3. Start the lccguard daemon

         sudo launchctl load /Library/LaunchDaemons/lccguard.plist


From now, on subsequent reboots, the lccguard daemon is launched automatically.
