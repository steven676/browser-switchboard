#!/bin/sh

# start Browser Switchboard, then start MicroB if appropriate
/usr/bin/browser-switchboard > /dev/null 2>&1 &

if ! /bin/pidof browser > /dev/null 2>&1; then
	DEFAULT_BROWSER=`/usr/bin/browser-switchboard-config -b`
	AUTOSTART_MICROB=`/usr/bin/browser-switchboard-config -o autostart_microb`
	if [ "$AUTOSTART_MICROB" -eq 1 ] || [ "$DEFAULT_BROWSER" = "microb" -a "$AUTOSTART_MICROB" -ne 0 ]; then
		/usr/bin/maemo-invoker /usr/bin/browser.launch > /dev/null 2>&1 &
	fi
fi
