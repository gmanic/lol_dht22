#!/bin/sh
#


automake_version=`automake --version | tr ' ' '\n' | egrep '^[0-9]\.[0-9a-z.-]+'`
if [ -z "$automake_version" ] ; then
    warn "unable to determine automake version"
else
    case $automake_version in
	0.*|1.[0-8]|1.[0-8][.-]*)
	    warn "automake ($automake_version) detected; 1.9 or newer recommended"
	    ;;
	*)
	    ;;
    esac
fi

set -ex

aclocal 
autoheader
autoconf
automake --add-missing --copy --foreign
