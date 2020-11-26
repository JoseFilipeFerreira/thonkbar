# 🤔 Thonkbar
yet another lemonbar wrapper modeled after [i3blocks](https://github.com/vivien/i3blocks)

## :link: Instalation
```bash
make install
```
* to launch the bar run the script `thonkbar_daemon`
* to restart the bar run the same script again

## ⚙️ Configure
The program reads a config file located at `~/.config/thonkbar/config`.

An example of this file can be found [here](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/config).

### bar side
Write one line with one of the following to define on wich side of the bar the
following blocks will be added

| options   | result               |
| --------- | -------------------- |
| [left]    | left side of screen  |
| [right]   | right side of screen |
| [center]  | center of the screen |

### block
To add a block first write the command to be run on the block and secondly the
type of delay to be used separated by a comma.

If the path to the command to be executed starts with `script/` the bar will look at the folder `~/.config/thonkbar/scripts`

There are two possibilities of outputs for the command:
* one line that is directly written on the block
* 3 lines every time it is run (the same format as i3blocks): long version, short version, color(#RRGGBB)


The delay can be any of the following
| options    | result                                                                                       |
| ---------- | -------------------------------------------------------------------------------------------- |
| int        | a number stating how often the script should be run                                          |
| ONCE       | run the script only once on startup                                                          |
| CONTINUOUS | listen to the script continuously and update the block everytime the script gives a new line |


### examples
* Config file that inserts a block on the right side that displays the time/date and runs every second and inserts a block on the left side that listens continuously to a script located at `~/.config/thonkbar/scripts/workspaces`
```ini
[right]
date '+%d/%m  %H:%M', 1
[left]
scripts/workspaces, CONTINUOUS
 ```
* Script for [battery](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/battery)
* Script for [wifi](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/wifi)
* Script for [workspaces on i3](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/workspaces).

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
