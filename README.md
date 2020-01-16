# nproxy - small proxy server
From time to time you have to debug applications with TCP protocols: MySQL, HTTP etc.
To simplify - made application - small proxy server, indicating transmitted data.

To build: use make

Command line options:

```
Use ./nproxy options, where options are:
-h or --help                              This help
-l <port> or --listen-port <port>         Set listen port (default 30101)
-c <port> or --connect-port <port>        Set connect port (default 30102)
-H <host> or --host <host>                Set connect host (default localhost)
-V or --version                           Print version and exit
-v or --verbose                           Set verbose mode
-s or --spy                               Set spy mode


```

Simple output:

```
xxx@ubuntu-home:~/work/nproxy$ ./nproxy -l 8080 -c 80 -H 82.14.20.33 -s
RX:47 45 54 20 2F 20 48 54 54 50 2F 31 2E 31 0D 0A  'GET / HTTP/1.1.. '
48 6F 73 74 3A 20 31 32 37 2E 30 2E 30 2E 31 3A  'Host: 127.0.0.1: '
38 30 38 30 0D 0A 55 73 65 72 2D 41 67 65 6E 74  '8080..User-Agent '
3A 20 4D 6F 7A 69 6C 6C 61 2F 35 2E 30 20 28 58  ': Mozilla/5.0 (X '
31 31 3B 20 55 62 75 6E 74 75 3B 20 4C 69 6E 75  '11; Ubuntu; Linu '
78 20 78 38 36 5F 36 34 3B 20 72 76 3A 37 32 2E  'x x86_64; rv:72. '
30 29 20 47 65 63 6B 6F 2F 32 30 31 30 30 31 30  '0) Gecko/2010010 '

```

