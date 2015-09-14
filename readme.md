Extract StatsD worker metrics from Apache.

This project includes two methods for extracting worker metrics from
Apache and dumping them into StatsD.

The first, and simplest, is `apachestatsd`, a Python script that hits
`/server-status?auto`, parses some common worker statistics, and gauges
them into StatsD.

However, having to perform an HTTP reqeust to examine the status of your
HTTP server is not always the best strategy.  Apache supports accessing
its "scoreboard" file through non-anonymous shared memory via the
following [configuration option][1].

    ScoreBoardFile /var/run/apache_runtime_status

Go into `asb` and run `make`.  This will build a binary
`apachescoreboard`, which reads the contents of the shared memory
structures and outputs worker statistics as key-value pairs to standard
out.  This standard out is ideal for piping into `gaugestatsd`, which
reads key-value pairs on standard in and gauges them to StatsD.

[1]: http://httpd.apache.org/docs/2.2/mod/mpm_common.html#scoreboardfile
