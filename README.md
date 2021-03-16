# 🤔 Thonkbar

yet another block based lemonbar wrapper

## :link: Instalation

```bash
make install
```

## ⚙️ Configure

The program reads a config file located at `~/.config/thonkbar/config`.

There are four fields that can be added to the config

| options   | result                             |
| --------- | ---------------------------------- |
| [config]  | general bar configs                |
| [left]    | blocks on the left side of screen  |
| [right]   | blocks on the right side of screen |
| [center]  | blocks on the center of the screen |

### general bar configs

```ini
[config]
delimiter = <delimiter>
delimiter_color = "#AARRGGBB"
font = "<font>:size=<integer>"
underline_width = <integer>
background_color = "#AARRGGBB"
foreground_color = "#AARRGGBB"
text_offset = <integer>
position = top|bottom
docking_mode = normal|force
```

### 📡 Create and Update blocks

To add a block first write the command to be run on the block and secondly the
type of delay to be used separated by a comma.

If the path to the command to be executed starts with `script/` the bar will
look at the folder `~/.config/thonkbar/scripts`

Scripts used can output up to 3 lines each time it is run:

* text on the block
* text colour (#RRGGBB)
* underline colour (#RRGGBB)

The delay can be any of the following
| options      | result                                                  |
| ------------ | --------------------------------------------------------|
| integer      | delay between executions in seconds                     |
| "ONCE"       | run the script only once on start-up                    |
| "CONTINUOUS" | update the block every time the script gives a new line |

Optionally a block can be updated via a signal. When you run the program
information is outputted about each block. This also includes the bound signal.

```bash
script: <script>
    update frequency: <time>
    signal: <signal_id>
```

To update a given block you can use the `pkill` program:

```bash
pkill --signal <signal_id> thonkbar
```

## Examples

* Config file that inserts a block on the right side that displays the time/date
and runs every second and inserts a block on the left side that listens
continuously to a script located at `~/.config/thonkbar/scripts/workspaces`

```ini
[config]
delimiter = "  |  "
delimiter_color = "#666666"

[right]
date '+%d/%m  %H:%M', 1

[left]
scripts/workspaces, CONTINUOUS
 ```

* My personal config can be found on my [dotfiles](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/config).
  * Script for [battery](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/battery)
  * Script for [wifi](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/wifi)
  * Script for [workspaces on i3](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/workspaces).

