# Ember

[![Join us on Gitter!](https://badges.gitter.im/Worldforge.svg)](https://gitter.im/Worldforge/Lobby)

[![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/ember)

Ember is the client for the [WorldForge](http://worldforge.org/ "The main Worldforge site") virtual world system.
It can be used for playing and exploring as well as authoring.

That means that if you're a player who wants to explore a virtual world hosted by a Worldforge server, this is the
client for you.
And if you're a world builder who wants to build your own world, this is also the client for you.

To learn more about Worldforge visit our [website](http://worldforge.org/ "The main Worldforge site").

### Navigation settings

The avatar's navigation mesh generation can be tuned through the configuration
keys `navigation.walkableclimb` and `navigation.walkableslopeangle`. These
control the maximum climb in voxels (default `100`) and the maximum walkable
slope angle in degrees (default `70`).

### Working with media

An alternative to the ```media-download-rsync``` target is to instead use the raw media repo source, as found at
https://svn.worldforge.org:886/svn/media/trunk/. If you intend to edit or add new media you probably want this instead.

The target ```mediarepo-checkout``` will use Subversion to checkout the repository to the ```mediarepo``` directory.
When running a non-release build Ember will automatically load media from this directory.
Note that this might lead to longer start times, as more files needs to be processed.

In addition, there's a ```mediarepo-process``` target which processes the media from the media repository, as fetched
by ```mediarepo-checkout```, and places the results in the build directory, under a subdirectory named
```ember-media-<version>```.
When running a non-release build Ember will automatically load media from this directory, taking precedence over the
```mediarepo``` directory.


## Running Ember

As Ember is a 3d client it requires a 3d capable graphics card. If you have a fairly recent machine you should be good
though.

If you want to run Ember as a world builder tool we strongly recommend that you also install and run a local instance of
the [Cyphesis](../cyphesis/README.md) server.
By running an instance of the Cyphesis server on your local machine you're automatically given administrative access to
the world, and can build and alter it however you like.

## How to help

If you're interested in helping out with development you should check out these resources:

* [The main Worldforge site](http://worldforge.org/ "The main Worldforge site")
* [Bugs and feature planning on Launchpad](https://launchpad.net/ember "Ember Launchpad entry")
* [Gitter conversation](https://gitter.im/Worldforge/Lobby "Gitter conversation")
* [Mailing lists](http://mail.worldforge.org/lists/listinfo/ "Mailing lists")
* [Ember wiki entry](http://wiki.worldforge.org/wiki/Ember "Ember wiki entry")
