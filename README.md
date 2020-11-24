# 🤔 Thonkbar
yet another lemonbar wrapper modeled after [i3blocks](https://github.com/vivien/i3blocks)

## :link: Instalation
```bash
make install
```
* to launch the bar run the script `thonkbar_daemon`
* to restart the bar run the same script again

## ⚙️ Configure
In the main function in [thonkbar.c](thonkbar.c) blocks can be inserted in the bar using the function
```C
insert_block(bar_area, block_comand, delay);
```

### bar_area

| options | result                            |
| ------- | --------------------------------- |
| left    | block on the left side of screen  |
| right   | block on the right side of screen |
| center  | block on the center of the screen |

### block_command
A string of the command to be executed. If the string starts with `script/` the bar will look at the folder `~/.config/thonkbar/scripts`
There are two possibilities of outputs for the command:
* one line that is directly written on the block
* 3 lines every time it is run (the same format as i3blocks): long version, short version, color(#RRGGBB)

Example scripts for [battery](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkscripts/battery), [wifi](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkscripts/wifi) and
 [workspaces for i3](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkscripts/workspaces).

### delay
how to update the block

| options    | result                                                                                       |
| ---------- | -------------------------------------------------------------------------------------------- |
| int        | a number stating how often the script should be run                                          |
| ONCE       | run the script only once on startup                                                          |
| CONTINUOUS | listen to the script continuously and update the block everytime the script gives a new line |


### Examples
* Insert a block on the right side that displays the time/date and runs every second
```C
insert_block(right, "date '+\%d/%m  %H:%M'", 1);
```
 * Insert a block on the left side that listens continuously to a script located at `~/.config/thonkbar/scripts/workspaces`
 ```C
 insert_block(left, "scripts/workspaces", CONTINUOUS);
 ```
## 📡 Update block
You can also update a block remotely with signals.
Blocks on the right side start at the signal RTMIN+1 and the signal increases with every new block
Every other block starts at RTMAX-1 and go down with every new block

To force the update of the first block inserted on the right side 
```bash
pkill -SIGRTMIN+1 thonkbar
```

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
