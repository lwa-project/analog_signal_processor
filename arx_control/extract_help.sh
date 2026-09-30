#!/bin/bash

helpname=`echo ${1} | sed -e 's/\.cpp/_help.h/g;'`
endl=`grep -e "\*\*\*/" -n ${1} | head -n1 | sed -e 's/:.*//g'`

if [[ "${endl}" != "" ]]; then
    endl=$((${endl} - 1))
    taill=$((${endl} - 1))
    echo -n "R\"HELP(" > ${helpname}
    head -n${endl} ${1} | tail -n${taill} >> ${helpname}
    echo -n ")HELP\"" >> ${helpname}
fi
