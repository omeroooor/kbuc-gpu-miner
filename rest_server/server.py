from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field, field_validator
from typing import Optional
import grpc
import sys
import time
import logging
sys.path.append('../client')  # Add client directory to path to import proto files
import miner_pb2
import miner_pb2_grpc

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(title="Mining REST API")

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Pydantic models for request/response
class StartMiningRequest(BaseModel):
    hash: str = Field(..., min_length=64, max_length=64, pattern=r"^[0-9a-fA-F]+$")
    addr1: str = Field(..., min_length=40, max_length=40, pattern=r"^[0-9a-fA-F]+$")
    addr2: str = Field(..., min_length=40, max_length=40, pattern=r"^[0-9a-fA-F]+$")
    value: int = Field(..., ge=0)
    timestamp: Optional[int] = None
    target: str = Field(default="000000ffffff0000000000000000000000000000000000000000000000000000", 
                       min_length=64, max_length=64, pattern=r"^[0-9a-fA-F]+$")
    time_limit: Optional[int] = Field(default=0, ge=0)
    flag: Optional[int] = Field(default=0, ge=0, le=1)  # Flag value must be 0 or 1

    @field_validator('hash', 'addr1', 'addr2', 'target')
    @classmethod
    def validate_hex(cls, v: str) -> str:
        try:
            int(v, 16)
            return v
        except ValueError:
            raise ValueError("Value must be a valid hexadecimal string")

class StartMiningResponse(BaseModel):
    session_id: str

class PauseMiningResponse(BaseModel):
    state_file: str

class ResumeMiningRequest(BaseModel):
    state_file: str

class ResumeMiningResponse(BaseModel):
    session_id: str

class StatusResponse(BaseModel):
    is_mining: bool
    current_nonce: str
    total_hashes: int = 0
    hash_rate: float = 0.0
    message: str = ""
    solution_found: bool = False
    solution_nonce: Optional[str] = None

# gRPC client
channel = grpc.insecure_channel('localhost:50051')
stub = miner_pb2_grpc.MinerServiceStub(channel)

@app.post("/mine/start", response_model=StartMiningResponse)
async def start_mining(request: StartMiningRequest):
    try:
        logger.info(f"Received mining request: {request.model_dump()}")
        
        # Set timestamp if not provided
        if request.timestamp is None:
            request.timestamp = int(time.time())
            
        # Set default time limit if not provided
        if request.time_limit is None:
            request.time_limit = 0
            
        # Create gRPC request
        grpc_request = miner_pb2.StartMiningRequest(
            hash=request.hash,
            addr1=request.addr1,
            addr2=request.addr2,
            value=request.value,
            timestamp=request.timestamp,
            target=request.target,
            time_limit=request.time_limit,
            flag=request.flag  # Include flag in gRPC request
        )
        
        # Call gRPC service
        try:
            logger.info("Calling gRPC StartMining service")
            response = stub.StartMining(grpc_request)
            if not response.success:
                error_msg = response.message or "Failed to start mining"
                logger.error(f"Mining service error: {error_msg}")
                raise HTTPException(status_code=400, detail=error_msg)
            if not response.session_id:
                logger.error("No session ID received from mining service")
                raise HTTPException(status_code=500, detail="No session ID received from mining service")
            logger.info(f"Mining started successfully with session ID: {response.session_id}")
            return {"session_id": response.session_id}
        except grpc.RpcError as e:
            status_code = e.code()
            error_msg = e.details() or str(e)
            if status_code == grpc.StatusCode.UNAVAILABLE:
                logger.error("Mining service is not available")
                raise HTTPException(status_code=503, detail="Mining service is not available. Is the gRPC server running?")
            logger.error(f"gRPC error: {error_msg}")
            raise HTTPException(status_code=500, detail=f"gRPC error: {error_msg}")
    except Exception as e:
        if isinstance(e, HTTPException):
            raise e
        logger.error(f"Unexpected error: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/mine/{session_id}/pause", response_model=PauseMiningResponse)
async def pause_mining(session_id: str):
    if not session_id or session_id.isspace():
        raise HTTPException(status_code=400, detail="Invalid session ID")
        
    try:
        logger.info(f"Received pause request for session ID: {session_id}")
        request = miner_pb2.PauseMiningRequest(session_id=session_id)
        response = stub.PauseMining(request)
        logger.info(f"Pause successful for session ID: {session_id}")
        return {"state_file": response.state_file}
    except grpc.RpcError as e:
        logger.error(f"gRPC error: {e.details()}")
        raise HTTPException(status_code=500, detail=f"gRPC error: {e.details()}")

@app.post("/mine/resume", response_model=ResumeMiningResponse)
async def resume_mining(request: ResumeMiningRequest):
    try:
        logger.info(f"Received resume request for state file: {request.state_file}")
        grpc_request = miner_pb2.ResumeMiningRequest(state_file=request.state_file)
        response = stub.ResumeMining(grpc_request)
        logger.info(f"Resume successful with session ID: {response.session_id}")
        return {"session_id": response.session_id}
    except grpc.RpcError as e:
        logger.error(f"gRPC error: {e.details()}")
        raise HTTPException(status_code=500, detail=f"gRPC error: {e.details()}")

@app.get("/mine/{session_id}/status", response_model=StatusResponse)
async def get_status(session_id: str):
    if not session_id or session_id.isspace():
        raise HTTPException(status_code=400, detail="Invalid session ID")
        
    try:
        logger.info(f"Received status request for session ID: {session_id}")
        request = miner_pb2.GetStatusRequest(session_id=session_id)
        response = stub.GetStatus(request)
        
        # Check if a solution was found
        solution_found = "Solution found" in response.message
        solution_nonce = response.current_nonce if solution_found else None
        
        logger.info(f"Status for session ID: {session_id} - is_mining: {response.is_mining}, current_nonce: {response.current_nonce}, total_hashes: {response.total_hashes}, hash_rate: {response.hash_rate}, message: {response.message}")
        return {
            "is_mining": response.is_mining,
            "current_nonce": response.current_nonce,
            "total_hashes": response.total_hashes,
            "hash_rate": response.hash_rate,
            "message": response.message,
            "solution_found": solution_found,
            "solution_nonce": solution_nonce
        }
    except grpc.RpcError as e:
        if "not found" in str(e.details()).lower():
            logger.error(f"Mining session not found for session ID: {session_id}")
            raise HTTPException(status_code=404, detail="Mining session not found")
        logger.error(f"gRPC error: {e.details()}")
        raise HTTPException(status_code=500, detail=f"gRPC error: {e.details()}")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8001)
