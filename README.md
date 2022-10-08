# 🤔 Thonkbar

minimalist block based lemonbar wrapper
## :link: Instalation

From the [AUR](https://aur.archlinux.org/packages/thonkbar-git/) or manualy
with:

```bash
make install
```

## ⚙️ Configure

The program reads a config file located at `~/.config/thonkbar/config`.

There are four fields that can be defined in the config file

| options   | result                              |
| --------- | ----------------------------------- |
| [config]  | general bar configs                 |
| [left]    | blocks on the left side of the bar  |
| [right]   | blocks on the right side of the bar |
| [center]  | blocks on the center of the bar     |

### General Bar Configs

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

To create a block you just need to add a line in one of the following formats:

```
<command>, <update>
<command>, <update>, <button handler>
```
Where:
#### \<script\>

Is a script that outputs a suported block format. If the path to the command to
be executed starts with `script/` the bar will look at the folder
`~/.config/thonkbar/scripts`

Scripts used can output up to 3 lines:
 - text on the block
 - text colour (#RRGGBB)
 - underline colour (#RRGGBB)

#### \<update\>

Represents the script update kind/frequency. Can be any of the following:

| options      | result                                                  |
| ------------ | --------------------------------------------------------|
| integer      | delay (greater than 0) between executions in seconds    |
| "ONCE"       | run the script only once on start-up                    |
| "CONTINUOUS" | update the block every time the script gives a new line |

Optionally a block can be updated via the corresponding signal via `pkill`.

Usefull information for each block (including the bound signal) is outputed when
the bar starts.

#### \<button handler\>
Is a script that is called with the following arguments whenever a mouse click
is registered on the bar.

```bash
<button handler> <button> <id>
```

where:
 - `<button>` can be: `LEFT`, `CENTER`, `RIGHT`, `UP` or `DOWN`
 - `<id>` is the signal bound to the block (usefull for updating the block after
     handling the button press)

## Examples

* Config file with:
  * custom delimiter character and color
  * block on the right side of the bar that displays time/date (updates every second)
  * block on the left side that listens continuously to a script located at `~/.config/thonkbar/scripts/workspaces`

```ini
[config]
delimiter = "  |  "
delimiter_color = "#666666"

[right]
date '+%d/%m  %H:%M', 1

[left]
scripts/workspaces, CONTINUOUS
 ```

* My personal [config](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/config)
* Usefull scripts:
  * Script for [battery](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/battery)
  * Script for [wifi](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/wifi)
  * Script for [workspaces on i3](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/workspaces).

