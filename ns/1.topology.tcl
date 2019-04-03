# Create simulation object
set ns [new Simulator]

# Ask ns for nodes
set n0 [$ns node]
set n1 [$ns node]

# Create a duplex link b/w n0 & n1
$ns duplex-link $n0 $n1 1Mb 10ms DropTail

# Schedule End
$ns at 5.0 "exit 0"

# Run Simulation
$ns run
