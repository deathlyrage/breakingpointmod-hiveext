# Breaking Point Mod (Hive Extension)

Source code to the C++ Hive Extension used for Database Intergration and many of the server side calls in Breaking Point Mod.

## Objectives & Goals
* Provide a better version of the hive extension for the servers still running
* Remove Hard Coded Usernames / Passwords / Server IP Addresses
* Remove Code that depends on the removed ( The Zombie Infection ) Community and Databases
* Support SQLite so people can setup servers easily without needing to run a full MySQL Instance
* Cross Platform Support ( Windows, Linux )
* 64 Bit Support ( Supports running the 64 Bit Arma 3 Server Executable allowing the arma 3 instance to allocate more memory )

## Conflicts & Problems

* Fragmentation: Everyone using a custom version of the Hive Extension and hosting vastly different servers and configurations. If we can work together and merge fixes together as a community.

## Automatic Updates

The Hive Extension will be automatically built by a build server when there is a commit or merge on github and deploy to a new version on steam. 
If you have new features to merge into Breaking Point that the community could use, please submit it as a pull requst and I'll do a new build and improve the mod for everyone.