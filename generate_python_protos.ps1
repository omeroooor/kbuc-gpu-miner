# Activate virtual environment
.\venv\Scripts\Activate.ps1

$protoPath = "proto/miner.proto"
$outputDir = "client"

# Create output directory if it doesn't exist
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir
}

# Install grpcio-tools if not already installed
pip install grpcio-tools

# Generate Python code from proto
python -m grpc_tools.protoc -I proto --python_out=client --grpc_python_out=client $protoPath

# Deactivate virtual environment
deactivate
