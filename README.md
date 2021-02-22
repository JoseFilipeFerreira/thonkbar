# 🤔 Thonkbar
yet another block based lemonbar wrapper

## :link: Instalation
```bash
make install
```
* to launch the bar run the script `thonkbar_daemon`
* to restart the bar run the same script again

## ⚙️ Configure
The program reads a config file located at `~/.config/thonkbar/config`.


There are four fields that can be edited on the config

| options   | result                             |
| --------- | ---------------------------------- |
| [config]  | general bar configs                |
| [left]    | blocks on the left side of screen  |
| [right]   | blocks on the right side of screen |
| [center]  | blocks on the center of the screen |

* config field
```ini
[config]
delimiter, [DELIMITER]
delimiter_color, #RRGGBB
```

### block
To add a block first write the command to be run on the block and secondly the
type of delay to be used separated by a comma.

If the path to the command to be executed starts with `script/` the bar will look at the folder `~/.config/thonkbar/scripts`

Scripts used can output up to 3 lines each time it is run:
* text on the block
* text color (#RRGGBB)
* underline color (#RRGGBB)

The delay can be any of the following
| options    | result                                                                                       |
| ---------- | -------------------------------------------------------------------------------------------- |
| int        | delay between executions in seconds                                                          |
| ONCE       | run the script only once on startup                                                          |
| CONTINUOUS | listen to the script continuously and update the block everytime the script gives a new line |


### Examples
* Config file that inserts a block on the right side that displays the time/date and runs every second and inserts a block on the left side that listens continuously to a script located at `~/.config/thonkbar/scripts/workspaces`
```ini
[config]
delimiter, |
delimiter_color, #666666

[right]
date '+%d/%m  %H:%M', 1

[left]
scripts/workspaces, CONTINUOUS
 ```

* My config can be found on my [dotfiles](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/config).
    * Script for [battery](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/battery)
    * Script for [wifi](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/wifi)
    * Script for [workspaces on i3](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/workspaces).

## 📡 Update block
You can also update a block remotely with signals.
Blocks on the right side start at the signal RTMIN+1 and the signal increases with every new block
Every other block starts at RTMAX-1 and go down with every new block

```bash
pkill --signal [SIGNAL ID] thonkbar
```

> `make debug` to get the signal id of a given block

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
