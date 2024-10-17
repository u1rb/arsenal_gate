import asyncio
from concurrent.futures import ProcessPoolExecutor
import time

# Example of a compute-heavy function (calculates the nth Fibonacci number)
def compute_fibonacci(n):
    if n <= 1:
        return n
    else:
        return compute_fibonacci(n-1) + compute_fibonacci(n-2)

# Asynchronous wrapper to run the compute-heavy function in a ProcessPoolExecutor
async def run_compute_heavy(executor, n):
    loop = asyncio.get_running_loop()
    result = await loop.run_in_executor(executor, compute_fibonacci, n)
    return result

async def main():
    # Define the numbers for which to compute Fibonacci numbers
    fibonacci_numbers = [35, 36, 37, 38, 39, 40]  # Adjust n for desired computation time

    # Create a ProcessPoolExecutor with a number of workers equal to the CPU cores
    with ProcessPoolExecutor() as executor:
        # Schedule all compute-heavy tasks concurrently
        tasks = [asyncio.create_task(run_compute_heavy(executor, n)) for n in fibonacci_numbers]

        # Optionally, show progress as tasks complete
        for coroutine in asyncio.as_completed(tasks):
            result = await coroutine
            print(f"Fibonacci result: {result}")

if __name__ == "__main__":
    start_time = time.time()
    asyncio.run(main())
    end_time = time.time()
    print(f"Total execution time: {end_time - start_time:.2f} seconds")