dnl $Id$
dnl config.m4 for extension smartcrop

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(smartcrop, for smartcrop support,
dnl Make sure that the comment is aligned:
dnl [  --with-smartcrop             Include smartcrop support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(smartcrop, whether to enable smartcrop support,
dnl Make sure that the comment is aligned:
[  --enable-smartcrop           Enable smartcrop support])

if test "$PHP_SMARTCROP" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-smartcrop -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/smartcrop.h"  # you most likely want to change this
  dnl if test -r $PHP_SMARTCROP/$SEARCH_FOR; then # path given as parameter
  dnl   SMARTCROP_DIR=$PHP_SMARTCROP
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for smartcrop files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       SMARTCROP_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$SMARTCROP_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the smartcrop distribution])
  dnl fi

  dnl # --with-smartcrop -> add include path
  dnl PHP_ADD_INCLUDE($SMARTCROP_DIR/include)

  dnl # --with-smartcrop -> check for lib and symbol presence
  dnl LIBNAME=smartcrop # you may want to change this
  dnl LIBSYMBOL=smartcrop # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $SMARTCROP_DIR/$PHP_LIBDIR, SMARTCROP_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_SMARTCROPLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong smartcrop lib version or lib not found])
  dnl ],[
  dnl   -L$SMARTCROP_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(SMARTCROP_SHARED_LIBADD)

  PHP_NEW_EXTENSION(smartcrop, smartcrop.c , $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
  PHP_ADD_EXTENSION_DEP(smartcrop, gd)
fi
