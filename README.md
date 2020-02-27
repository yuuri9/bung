The contained comprises the beginnings of a meguca v1 client for 9front. 
Implemented are processes for reading the JSON API, and extracting the JSON object from main page of board which exports site-specific configuration information.
An ryo websocket implementation as threads is present and comprises the current bulk of donework, it can be tested by
```
	mk
	dialr
```
Then in a seperate terminal
```
	con -C /srv/dialr.cmd
	connect {"site" : "daijoubu.cf"}
```

will be echoed.
as well will be seen the traffic of pings

expect for altogether too many debug text
