#!/bin/csh
#___INFO__MARK_BEGIN__
##########################################################################
#
#  The Contents of this file are made available subject to the terms of
#  the Sun Industry Standards Source License Version 1.2
#
#  Sun Microsystems Inc., March, 2001
#
#
#  Sun Industry Standards Source License Version 1.2
#  =================================================
#  The contents of this file are subject to the Sun Industry Standards
#  Source License Version 1.2 (the "License"); You may not use this file
#  except in compliance with the License. You may obtain a copy of the
#  License at http://www.gridengine.sunsource.net/license.html
#
#  Software provided under this License is provided on an "AS IS" basis,
#  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
#  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
#  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
#  See the License for the specific provisions governing your rights and
#  obligations concerning the Software.
#
#  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
#
#  Copyright: 2001 by Sun Microsystems, Inc.
#
#  All Rights Reserved.
#
##########################################################################
#___INFO__MARK_END__
setenv SOURCE_ROOT "../../../source"

setenv SOURCE_D "{$SOURCE_ROOT,$SOURCE_ROOT/*/,$SOURCE_ROOT/*/*/,$SOURCE_ROOT/*/*/*/,$SOURCE_ROOT/*/*/*/*/,$SOURCE_ROOT/*/*/*/*/*/,$SOURCE_ROOT/*/*/*/*/*/*/,$SOURCE_ROOT/*/*/*/*/*/*/*/}"
setenv SOURCE_C "$SOURCE_D/*.c"
setenv SOURCE_H "$SOURCE_D/*.h"

adoc --no-warnings -b -DPROJECT "Grid Engine Source" -DEDITION "" -DREVISION "" -DCOPYRIGHT "2001 by Sun Microsystems, Inc." -DAUTHOR "" -I -xon -xitemize  -2 $SOURCE_C $SOURCE_H > "source.texi"
