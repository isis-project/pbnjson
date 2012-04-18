#
# Regular cron jobs for the pbnjson package
#
0 4	* * *	root	[ -x /usr/bin/pbnjson_maintenance ] && /usr/bin/pbnjson_maintenance
