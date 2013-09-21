#!/bin/bash

SENDTO=""
ACCOUNT="default"
NUMBER=""
AREA=""
AREACODE=""
NAME=""
TIME=""
MSN=""
SERVICE=""
ALIAS=""
FIX=""

TEMP=`getopt -o n:a:A:N:t:m:s:r:f: --long \
	number:,area:,areacode:,name:,time:,msn:,service:,alias:,fix:,account:,sendto: \
	-n '$0' -- "$@"`

if [ $? != 0 ] ; then echo "Error parsing options" >&2 ; exit 1 ; fi

eval set -- "$TEMP"

while true
do
        case $1 in
                -n|--number) NUMBER=$2 ; shift 2;;
                -a|--area) AREA=$2 ; shift 2;;
                -A|--areacode) AREACODE=$2 ; shift 2;;
                -N|--name) NAME=$2 ; shift 2;;
                -t|--time) TIME=$2 ; shift 2;;
                -m|--msn) MSN=$2 ; shift 2;;
                -s|--service) SERVICE=$2 ; shift 2;;
                -r|--alias) ALIAS=$2 ; shift 2;;
                -f|--fix) FIX=$2 ; shift 2;;
		--account) ACCOUNT=$2; shift 2;;
		--sendto) SENDTO=$2; shift 2;;
		--) shift; break ;;
		*) echo "Internal error (found $1)" >&2 ; exit 1;;
        esac
done

zeit=$(date -d "${TIME}" +"am %A, %d. %B %Y um %H:%M:%S")

if [ -z $SENDTO ]
then
	echo "No address to send to" >&2;
	exit 1;
fi

echo "Neuer Anruf von

  (${AREACODE}) ${NUMBER} ($AREA)
  $NAME

an ${MSN} (${ALIAS})
${zeit}" | mailx -s "Neuer Anruf von (${AREACODE}) ${NUMBER}" -a "Content-Type: text/plain; charset=UTF-8" ${SENDTO} -- --account=${ACCOUNT}
