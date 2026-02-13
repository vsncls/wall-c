# fish completion for wall-c
#
# Supports short and long options.

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
        awk '
            {
                sub(/[[:space:]]*#.*/, "", $0);
                if ($0 ~ /^[[:space:]]*$/) next;
                n = split($0, a, /[[:space:]]+/);
                if (n == 0) next;
                if (a[1] ~ /^([[:xdigit:]]{2}[:-]){5}[[:xdigit:]]{2}$/ || a[1] ~ /^[[:xdigit:]]{12}$/) {
                    print a[1];
                } else if (n >= 2) {
                    print a[2];
                } else {
                    print a[1];
                }
            }
        ' "$cfg"
    end
end

function __wall_c_targets_from_config
    set -l cfg (__wall_c_config_path)
    if test -n "$cfg" -a -r "$cfg"
        awk '
            {
                sub(/[[:space:]]*#.*/, "", $0);
                if ($0 ~ /^[[:space:]]*$/) next;
                n = split($0, a, /[[:space:]]+/);
                if (n < 2) next;
                if (a[1] ~ /^([[:xdigit:]]{2}[:-]){5}[[:xdigit:]]{2}$/ || a[1] ~ /^[[:xdigit:]]{12}$/) next;
                print a[1];
            }
        ' "$cfg"
    end
end

complete -c wall-c -s h -d "Show help message"
complete -c wall-c -s y -d "Skip confirmation prompt"
complete -c wall-c -l help -d "Show help message"
complete -c wall-c -l yes -d "Skip confirmation prompt"
complete -c wall-c -l version -d "Show version"
complete -c wall-c -l dry-run -d "Validate and print without sending"
complete -c wall-c -l quiet -d "Reduce normal output"
complete -c wall-c -l smart -d "Skip send if host appears awake and verify after send"
complete -c wall-c -l continue-on-error -d "Continue after failures in batch mode"
complete -c wall-c -l list-targets -d "List configured targets"
complete -c wall-c -s b -r -d "Broadcast IPv4 address"
complete -c wall-c -s p -r -d "Destination UDP port"
complete -c wall-c -s m -r -d "Target MAC address" -a "(__wall_c_macs_from_config)"
complete -c wall-c -l broadcast -r -d "Broadcast IPv4 address"
complete -c wall-c -l interface -r -d "Resolve broadcast from interface"
complete -c wall-c -l port -r -d "Destination UDP port"
complete -c wall-c -l mac -r -d "Target MAC address" -a "(__wall_c_macs_from_config)"
complete -c wall-c -l target -r -d "Named config target" -a "(__wall_c_targets_from_config)"
complete -c wall-c -l count -r -d "Repeat sends per target"
complete -c wall-c -l interval-ms -r -d "Delay between repeats in ms"
