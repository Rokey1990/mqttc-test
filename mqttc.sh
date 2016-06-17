if [ ! -n "`which gcc`" ]; then
    echo "the gcc required!"
else
    echo "begin compile the mqttc..."
fi

if [ -n "$1" ]; then
    echo "\nconfig file: $1"
else
    echo "\nconfig file: mqttc/config.cfg"
fi
if [ -n "$2" ]; then
    echo "client type: $2\n"
else
    echo "client type: sub\n"
fi

ulimit -c unlimited

if [ -d "log" ]; then
    rm -f log/*.log
fi

rm -f *.log
rm -f mqttc/*.o
rm -f Mqtt_c
gcc -lpthread mqttc/*.c -o Mqtt_c
echo "begin run the mqttc..."
if [ -n "$1" ]; then
    if [ -n "$2" ];then
        ./Mqtt_c $1 $2
    else
        ./Mqtt_c $1
    fi

else
./Mqtt_c mqttc/config.cfg
fi
