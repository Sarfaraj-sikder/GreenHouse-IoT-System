#!/bin/bash


# Define output filename based on current date
output_filename="Output_$(date +'%Y-%m-%d').csv"


# Query data from InfluxDB and save to CSV file

docker exec influxdb influx -database 'SensorDB' -precision rfc3339 -execute "SELECT * FROM SmartSensors WHERE time >= now() -24h" -format csv | awk 'BEGIN{FS=OFS=","} {for(i=1; i<=NF; i++) {if($i=="") $i="NA"}; print}' > ./CSV_files/"$output_filename"