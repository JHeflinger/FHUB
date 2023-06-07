# FHUB

FHUB is a server/client program to host group communication and file hosting. Using TCP and UDP web sockets in C, FHUB
features a server program that can communicate and handle multiple client programs at once that can interface with FHUB
as a group chat or file hosting service. The purpose of FHUB is to provide a virtual wrapper around a root directory to
easily and immediately set up a small cloud hosting service.

## Features

FHUB includes but is not limited to the following features:
- Intuitive command line interface
- Group communication
- Flexible hosting and connection services
- File hosting
- File transfer
- File system navigation

## Building

You can either use our prebuilt binaries on our release page (currently not out until v1.0), or build using our source code located in `FHUB/`!
To build the program, it's fairly simple to figure out yourself. Note that this program is meant for windows users (sorry linux),
so it'll only compile on windows devices. If you have `gcc`, then you can also navigate to the `Scripts/` folder and run the respective
build scripts! Note that these scripts run off of the relative location, so make sure you're in the directory so they work correctly!

## How to Use

FHUB has two programs:

### Server

To use the FHUB server, put the executable program in the same directory as your root filesystem folder. Then, rename your
desired filesystem root to `ROOT` and then run the program! You will be prompted to run the program on a desired port, or you 
can keep the default. Once finished, you're done! You will also be able to interface with the server via the command line. For
a list of commands, use the `/help` command.

Note that if you don't have a desired filesystem to start with, no worries! The server will run just fine, and as soon as someone
tries to access the filesystem a root folder will be automatically created for you.

### Client

To use the FHUB client, simply run the client executable and fill out the prompted information. You will be promped for an IP address and port to
connect to, as well as a username. Don't worry, this isn't some account you have to create and authenticate. Your username will simply be an
identifier for others if used as a group chat. Once you're set up and connected, you'll be able to chat or run commands to interface with the filesystem!
To check out a list of commands, type in the `/help` command!