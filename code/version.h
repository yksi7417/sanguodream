#ifndef HEXDOM_VERSION_H
#define HEXDOM_VERSION_H

/* The version of hexdom and the number of the build. The title menu and the
   help window show them.
   - HEXDOM_VERSION changes by hand at a release.
   - build.bat and build_web.bat give HEXDOM_BUILD_NUMBER, the count of the
     commits of git, and HEXDOM_BUILD_COMMIT, the short hash of the commit
     (misc/build_number.bat). A build without git gives 0 and "dev".
   The two numbers come in as bare tokens and the macros make the text. Thus
   the build scripts need no quotes around them. */
#define HEXDOM_VERSION "0.1.0"

#ifndef HEXDOM_BUILD_NUMBER
#define HEXDOM_BUILD_NUMBER 0
#endif
#ifndef HEXDOM_BUILD_COMMIT
#define HEXDOM_BUILD_COMMIT dev
#endif

#define HEXDOM_TEXT_OF_(token) #token
#define HEXDOM_TEXT_OF(token)  HEXDOM_TEXT_OF_(token)
#define HEXDOM_BUILD_TEXT      HEXDOM_TEXT_OF(HEXDOM_BUILD_NUMBER) " (" HEXDOM_TEXT_OF(HEXDOM_BUILD_COMMIT) ")"

#endif
