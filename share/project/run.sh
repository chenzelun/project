#!/bin/bash
./baker
./salesman &
while [ 1 ]
do
      sleep 1
      ./customer
done
