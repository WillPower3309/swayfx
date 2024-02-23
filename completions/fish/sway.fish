# swayfx(1) completion

complete -f -c swayfx
complete -c swayfx -s h -l help --description "Show help message and quit."
complete -c swayfx -s c -l config --description "Specifies a config file." -r
complete -c swayfx -s C -l validate --description "Check the validity of the config file, then exit."
complete -c swayfx -s d -l debug --description "Enables full logging, including debug information."
complete -c swayfx -s v -l version --description "Show the version number and quit."
complete -c swayfx -s V -l verbose --description "Enables more verbose logging."
complete -c swayfx -l get-socketpath --description "Gets the IPC socket path and prints it, then exits."

