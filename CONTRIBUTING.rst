Contributing
============

Quick Links for Contributing
----------------------------

 - Compiling and building OBS Studio:
   https://github.com/obsproject/obs-studio/wiki/Install-Instructions

 - Our bug tracker:
   https://github.com/obsproject/obs-studio/issues

 - Discord Server: https://obsproject.com/discord

 - Development chat: #development on the Discord server (see above)
 
 - Development forum:
   https://obsproject.com/forum/list/general-development.21/

 - Developer/API Documentation:
   https://obsproject.com/docs

 - To contribute language translations, do not make pull requests.
   Instead, use crowdin.  Read here for more information:
   https://obsproject.com/forum/threads/how-to-contribute-translations-for-obs.16327/

Coding Guidelines
-----------------

 - OBS Studio uses kernel normal form (linux variant), for more
   information, please read here:
   https://github.com/torvalds/linux/blob/master/Documentation/process/coding-style.rst

 - Avoid trailing spaces.  To view trailing spaces before making a
   commit, use "git diff" on your changes.  If colors are enabled for
   git in the command prompt, it will show you any whitespace issues
   marked with red.

 - Tabs for indentation, spaces for alignment.  Tabs are treated as 8
   columns wide.

 - 80 columns max

Commit Guidelines
-----------------

 - OBS Studio uses the 50/72 standard for commits.  50 characters max
   for the title (excluding module prefix), an empty line, and then a
   full description of the commit, wrapped to 72 columns max.  See this
   link for more information: http://chris.beams.io/posts/git-commit/

 - Make sure commit titles are always in present tense, and are not
   followed by punctuation.

 - Prefix each commit's titles with the module name, followed by a colon
   and a space (unless modifying a file in the base directory).  After
   that, the first word should be capitalized.

   So for example, if you are modifying the obs-ffmpeg plugin::

     obs-ffmpeg: Fix bug with audio output

   Or for libobs::

     libobs: Fix source not displaying

   Note: When modifying cmake modules, just prefix with "cmake".

 - If you still need examples, please view the commit history.
