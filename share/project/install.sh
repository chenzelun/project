#!/bin/bash
make
cp ./baker /etc/init.d/
cp ./bakerd.conf /etc/
echo /etc/init.d/baker >> /etc/rc.d/rc.local

