# Activate virtual environment
..\venv\Scripts\Activate.ps1

$protoPath = "..\proto\miner.proto"
$outputDir = "."

# Create output directory if it doesn't exist
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir
}

# Install grpcio-tools if not already installed
pip install grpcio-tools

# Generate Python code from proto
python -m grpc_tools.protoc -I ..\proto --python_out=. --grpc_python_out=. $protoPath

# Deactivate virtual environment
deactivate
