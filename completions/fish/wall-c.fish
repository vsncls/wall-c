# fish completion for wall-c
#
# Supports the current short options exposed by the CLI:
#   -m <mac>, -b <broadcast_ip>, -p <port>, -y, -h

function __wall_c_config_path
    if set -q XDG_CONFIG_HOME
        echo "$XDG_CONFIG_HOME/wall-c/config"
    else if set -q HOME
        echo "$HOME/.config/wall-c/config"
    end
end

function __wall_c_macs_from_config
    set -l cfg (__wall_c_config_path)
    if test -n "$cfg" -a -r "$cfg"
        # Emit non-empty, non-comment lines as completion candidates.
        sed -E 's/[[:space:]]*#.*$//' "$cfg" | sed -E '/^[[:space:]]*$/d'
    end
end

complete -c wall-c -s h -d "Show help message"
complete -c wall-c -s y -d "Skip confirmation prompt"
complete -c wall-c -s b -r -d "Broadcast IPv4 address"
complete -c wall-c -s p -r -d "Destination UDP port"
complete -c wall-c -s m -r -d "Target MAC address" -a "(__wall_c_macs_from_config)"
