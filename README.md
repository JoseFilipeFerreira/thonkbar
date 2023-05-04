# 🤔 Thonkbar

minimalist block based lemonbar wrapper

---

## :link: Instalation

From the [AUR](https://aur.archlinux.org/packages/thonkbar-git/) with:
```bash
aura -S thonkbar-git
```
or manualy with:

```bash
make install
```

---

## ⚙️ Configure

The program reads a config file located at `~/.config/thonkbar/config`.


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
left_padding = <integer>
right_padding = <integer>
position = top|bottom
docking_mode = normal|force
```

### 📡 Create and Update blocks

To create a block you just need to add a line in one of the following formats:

```ini
[name]
essential = True|False
side = left|right|center
cmd = <command>
update = CONTINUOUS|ONCE|<integer>
event = <command>
```

#### \<command\>

Can be either a command in the `$PATH` or a command in
`~/.config/thonkbar/scripts` (when prepended with `scripts/`) that outputs a suported block format.

#### essential

Using the `SIGUSR2` a user can toggle the bar between showing all the blocks
and showing only the blocks marked as essential. By default the blocks are
marked as essential so this field can be omitted.

#### cmd

Can be either a command in the `PATH` or a command in
`~/.config/thonkbar/scripts` (when prepended with `scripts/`) that outputs a supported block format.

Commands used can output up to 3 lines:
 - text on the block
 - text color (#RRGGBB)
 - underline color (#RRGGBB)

#### update

Represents the script update kind/frequency. Can be any of the following:

| options      | result                                                  |
| ------------ | --------------------------------------------------------|
| integer      | delay (greater than 0) between executions in seconds    |
| "ONCE"       | run the script only once on start-up                    |
| "CONTINUOUS" | update the block every time the script gives a new line |

Optionally a block can be updated via the corresponding signal via `pkill`.

Useful information for each block (including the bound signal) is outputted when
the bar starts.

#### event
Is a script that is called with the following arguments whenever a mouse click
is registered on the bar.

```bash
<button handler> <button> <id>
```

 - `<button>` can be: `LEFT`, `CENTER`, `RIGHT`, `UP` or `DOWN`
 - `<id>` is the signal bound to the block (useful for updating the block after
     handling the button press)

---

## Examples

* Config file with:
  * custom delimiter character and color
  * block on the right side of the bar that displays time/date (updates every second)
  * block on the left side that listens continuously to a script located at `~/.config/thonkbar/scripts/workspaces`

```ini
[config]
delimiter = "  |  "
delimiter_color = "#666666"

[uptime]
side = right
cmd = "uptime -p"
update = 60

[workspaces]
side = left
cmd = "scripts/workspaces"
update = CONTINUOUS
 ```

* My personal [config](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/config)
* Usefull scripts:
  * Script for [battery](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/battery)
  * Script for [wifi](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/wifi)
  * Script for [workspaces on i3](https://github.com/JoseFilipeFerreira/toolbelt/blob/master/powertools/thonkbar/scripts/workspaces).

---

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
