print("1. Loading System Libraries...")
import sys
import os
import time
import collections

print("2. Loading Plotting Libraries (This might take a moment)...")
import matplotlib
# Force the standard Windows backend to prevent silent crashes
matplotlib.use('TkAgg') 
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.image as mpimg

print("3. Loading File Watcher...")
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

# --- CONFIGURATION ---
WATCH_DIR = "."
WINDOW_SIZE = 60 

# --- GLOBAL STATE ---
latest_image_path = None
throughput_history = collections.deque(maxlen=WINDOW_SIZE)
timestamps = collections.deque(maxlen=WINDOW_SIZE)
byte_counter = 0
last_tick = time.time()

class QuasarHandler(FileSystemEventHandler):
    def on_created(self, event):
        global latest_image_path, byte_counter
        if not event.is_directory:
            filename = event.src_path
            if "rx_" in filename and filename.endswith(".pgm"):
                try:
                    time.sleep(0.05) 
                    size = os.path.getsize(filename)
                    byte_counter += size
                    latest_image_path = filename
                    print(f"[Event] New Data: {filename} ({size/1024:.1f} KB)")
                except:
                    pass

def update_dashboard(frame):
    global byte_counter, last_tick, img_plot
    
    current_time = time.time()
    
    # Update Graph
    if current_time - last_tick >= 1.0:
        mbps = (byte_counter * 8) / (1024 * 1024) 
        throughput_history.append(mbps)
        timestamps.append(len(throughput_history))
        byte_counter = 0
        last_tick = current_time
        
        line_graph.set_data(timestamps, throughput_history)
        ax_graph.set_xlim(0, max(60, len(timestamps)))
        ax_graph.set_ylim(0, max(1.0, max(throughput_history) * 1.2))

    # Update Image
    if latest_image_path:
        try:
            img = mpimg.imread(latest_image_path)
            if img_plot is None:
                img_plot = ax_video.imshow(img, cmap='gray')
            else:
                img_plot.set_data(img)
        except:
            pass 

if __name__ == "__main__":
    print("4. Initializing GUI...")
    try:
        fig = plt.figure(figsize=(10, 5), facecolor='#1e1e1e')
        fig.canvas.manager.set_window_title('Quasar Mission Control')

        ax_video = fig.add_subplot(1, 2, 1)
        ax_video.set_title("Live Saliency Feed", color='white')
        ax_video.axis('off')
        img_plot = None

        ax_graph = fig.add_subplot(1, 2, 2, facecolor='#2d2d2d')
        ax_graph.set_title("Bandwidth (Mbps)", color='white')
        ax_graph.grid(True, color='#444444')
        line_graph, = ax_graph.plot([], [], color='#00ff00', linewidth=2)

        observer = Observer()
        observer.schedule(QuasarHandler(), path=WATCH_DIR, recursive=False)
        observer.start()
        
        print("5. Launching Window... (Check your taskbar if it doesn't pop up)")
        ani = animation.FuncAnimation(fig, update_dashboard, interval=100) 
        plt.show()
        
        observer.stop()
        observer.join()
    except Exception as e:
        print(f"CRITICAL ERROR: {e}")
        input("Press Enter to exit...")