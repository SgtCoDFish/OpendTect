#!/bin/csh
#(C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
# Description:  CMake script to define dgbssis package variables
# Author:       Nageswara
# Date:         August 2012
#RCS:           $Id$

#//TODO Modify script to work on all platforms.
SET( LIBLIST SSIS uiSSIS )
SET( EXECLIST od_extract_horizon od_process_flattening od_process_wheelercube )
#SET( PACKAGE_DIR "/dsk/d21/nageswara/dev/buildtest/lux64" )
SET( PACK "dgbssis" )
