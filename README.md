# Breaking Point Mod (Hive Extension)

Source code to the C++ Hive Extension used for Database Intergration and many of the server side calls in Breaking Point Mod.

## Todo
* Support SQLite so people can setup servers easily without needing to run a full MySQL Instance
* Cross Platform Support ( Windows, Linux )

## Current Features
* 64 Bit Support ( Supports running the 64 Bit Arma 3 Server Executable allowing the arma 3 instance to allocate more memory )
* Multi-threaded & Async Hive Extension that is non-blocking
* Supports Priorty Calls both High & Low. High Priorty Calls will run first.
* Queue System that allows scheduling and checking if a sql call is finished or not
* Fire Daemon Support ( Allows for Automatic Restarting / Running as a Service )
* Rcon Support ( Kicking Players, Validation, Whitelisting of Players )
* Steam Web API Support ( Vac Queries etc )
* Automatic Server Restarts + Warnings through hive extension connecting locally though rcon
* Threading Settings ( Determine how much CPU Usage the Hive Extension will use and how often it will sleep between calls )

## Conflicts & Problems

* Fragmentation: Everyone using a custom version of the Hive Extension and hosting vastly different servers and configurations. If we can work together and merge fixes together as a community.

## Automatic Updates

The Hive Extension will be automatically built by a build server when there is a commit or merge on github and deploy to a new version on steam. 
If you have new features to merge into Breaking Point that the community could use, please submit it as a pull requst and I'll do a new build and improve the mod for everyone.

## Credits

* Deathlyrage / Breaking Point Mod Team / Alderon Games Pty Ltd
* KamikazeXeX
* Rajko Stojadinovic
* Cameron Desrochers
* Prithu "bladez" Parker
* Declan Ireland
* MySQL Team + Oracle
* Poco C++ Lib Team
* Boost C++ Team