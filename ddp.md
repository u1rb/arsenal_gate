```python
# main.py
import os
import torch
import torch.nn as nn
import torch.optim as optim
import torch.distributed as dist
import torch.multiprocessing as mp
from torch.nn.parallel import DistributedDataParallel as DDP
import argparse

def setup(rank, world_size, master_addr, master_port):
    """Initializes the distributed environment."""
    os.environ['MASTER_ADDR'] = master_addr
    os.environ['MASTER_PORT'] = master_port
    # Initialize the process group
    # Use 'nccl' backend for NVIDIA GPUs
    dist.init_process_group("nccl", rank=rank, world_size=world_size)
    torch.cuda.set_device(rank % torch.cuda.device_count()) # Set device for this process

def cleanup():
    """Destroys the process group."""
    dist.destroy_process_group()

class ToyModel(nn.Module):
    """A simple model for demonstration."""
    def __init__(self):
        super(ToyModel, self).__init__()
        self.net1 = nn.Linear(10, 10)
        self.relu = nn.ReLU()
        self.net2 = nn.Linear(10, 5)

    def forward(self, x):
        return self.net2(self.relu(self.net1(x)))

def demo_basic(rank, world_size, args):
    """Main DDP training function."""
    print(f"Running basic DDP example on rank {rank}.")
    setup(rank, world_size, args.master_addr, args.master_port)

    # Create model and move it to GPU with id rank
    # Important: model must be on the correct device *before* DDP wrapping
    local_gpu_id = rank % torch.cuda.device_count()
    model = ToyModel().to(local_gpu_id)
    ddp_model = DDP(model, device_ids=[local_gpu_id])

    loss_fn = nn.MSELoss()
    optimizer = optim.SGD(ddp_model.parameters(), lr=0.001)

    print(f"Rank {rank}, Device {local_gpu_id}: Model setup complete.")

    # Simple training loop
    for epoch in range(args.epochs):
        # Simulate data loading for each process
        # In a real scenario, use DistributedSampler with DataLoader
        inputs = torch.randn(20, 10).to(local_gpu_id)
        labels = torch.randn(20, 5).to(local_gpu_id)

        optimizer.zero_grad()
        outputs = ddp_model(inputs)
        loss = loss_fn(outputs, labels)
        loss.backward() # DDP handles gradient averaging
        optimizer.step()

        if epoch % 10 == 0:
             # Ensure only rank 0 prints to avoid log spamming, or use a proper logger
             if rank == 0:
                 print(f"Rank {rank}, Epoch {epoch}, Loss: {loss.item()}")

        # Use dist.barrier() to sync processes if needed, e.g., before evaluation
        # dist.barrier()

    print(f"Rank {rank}: Training finished.")
    cleanup()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='PyTorch DDP Example')
    parser.add_argument('--local_rank', type=int, default=-1,
                        help='Local rank for distributed training')
    parser.add_argument('--master_addr', type=str, default='127.0.0.1', # Often the IP of Node 0
                        help='Master node address')
    parser.add_argument('--master_port', type=str, default='29500', # An unoccupied port
                        help='Master node port')
    parser.add_argument('--world_size', type=int, required=True,
                        help='Total number of processes (GPUs) across all nodes')
    parser.add_argument('--rank', type=int, required=True,
                        help='Global rank of this process')
    parser.add_argument('--epochs', type=int, default=50, help='Number of training epochs')
    args = parser.parse_args()

    # Note: In modern PyTorch (1.9+), using torchrun often handles
    # setting rank and world_size based on environment variables like
    # LOCAL_RANK, RANK, WORLD_SIZE, MASTER_ADDR, MASTER_PORT.
    # The explicit args above are for clarity or manual launching.
    # If using torchrun, you might not need to pass rank/world_size explicitly.

    demo_basic(args.rank, args.world_size, args)


```

```
# This command launches 4 processes on this machine (GPUs 0, 1, 2, 3)
# These processes will have global ranks 0, 1, 2, 3
export CUDA_VISIBLE_DEVICES=0,1,2,3 # Ensure correct GPUs are visible if needed
torchrun \
    --nproc_per_node=4 \
    --nnodes=2 \
    --node_rank=0 \
    --master_addr="192.168.1.100" \
    --master_port=29500 \
    main.py --epochs 50
    # Add any other script arguments here if needed (like --batch_size, etc.)
    # Note: torchrun automatically sets RANK, WORLD_SIZE based on these args
```

```
# This command launches 4 processes on this machine (GPUs 0, 1, 2, 3 - locally)
# These processes will have global ranks 4, 5, 6, 7
export CUDA_VISIBLE_DEVICES=0,1,2,3 # Ensure correct GPUs are visible if needed
torchrun \
    --nproc_per_node=4 \
    --nnodes=2 \
    --node_rank=1 \
    --master_addr="192.168.1.100" \
    --master_port=29500 \
    main.py --epochs 50
    # Arguments should match the master node's launch command

```

```
uv pip install torch torchvision torchaudio --extra-index-url https://download.pytorch.org/whl/cu121
uv pip install torch torchvision torchaudio --extra-index-url https://download.pytorch.org/whl/cu118

python -c "import torch; print(torch.__version__); print(torch.cuda.is_available())"

torchrun --version

```
