import serial
import threading
import time
import pygame
import sys
from AnimatedGIF import AnimatedGIF  # NEW: Import your standalone class file
from UI import draw_ui

# =======================================================================
# CONFIGURATION
# =======================================================================
SERIAL_PORT = "COM3" 
BAUD_RATE = 115200
GIF_FILENAME = "assets\\waterfall temple background.gif"  # NEW: Path to your background GIF

# Colors
BLACK = (20, 20, 20)
WHITE = (240, 240, 240)
BLUE = (50, 100, 200)       
BLUE_BRIGHT = (100, 200, 255) 
RED = (200, 50, 50)         
RED_BRIGHT = (255, 100, 100)  
GREEN = (50, 255, 50)
YELLOW = (255, 200, 50)
GRAY = (100, 100, 100)
ROPE_COLOR = (200, 180, 140)

MAX_SCORE_DIFF = 100 

# --- SHARED GLOBAL VARIABLES ---
game_data = {
    "p1_score": 0,
    "p2_score": 0,
    "pattern_state": 0, 
    "game_state": 0,    
    "p1_calib": 0, 
    "p2_calib": 0,
    "p1_is_flexing": 0, # NEW: True state from ESP32
    "p2_is_flexing": 0  # NEW: True state from ESP32
}

calib_timers = {
    "p1_start_time": 0.0,
    "p2_start_time": 0.0,
    "p1_last_state": 0,
    "p2_last_state": 0
}




# =======================================================================
# BACKGROUND THREAD: Pure Serial Listener
# =======================================================================
def serial_worker():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to ESP32 on {SERIAL_PORT}")
    except Exception as e:
        print(f"CRITICAL ERROR: Could not open {SERIAL_PORT}.")
        return 

    while True:
        if ser.in_waiting > 0:
            try:
                raw_line = ser.readline().decode('utf-8').strip()
                if raw_line.startswith("<") and raw_line.endswith(">"):
                    clean_line = raw_line[1:-1]
                    parts = clean_line.split(',')
                    
                    # NEW: Expecting 8 parts instead of 6!
                    if len(parts) == 8:
                        game_data["p1_score"] = int(parts[0])
                        game_data["p2_score"] = int(parts[1])
                        game_data["pattern_state"] = int(parts[2])
                        game_data["game_state"] = int(parts[3])
                        game_data["p1_calib"] = int(parts[4])
                        game_data["p2_calib"] = int(parts[5])
                        game_data["p1_is_flexing"] = int(parts[6]) # 1 or 0
                        game_data["p2_is_flexing"] = int(parts[7]) # 1 or 0
            except Exception as e:
                pass 


# =======================================================================
# MAIN LOOP
# =======================================================================
def main():
    pygame.init()
    screen = pygame.display.set_mode((0, 0), pygame.FULLSCREEN)
    pygame.display.set_caption("EMG Tug Of War - Display Client")
    clock = pygame.time.Clock()
    WIDTH, HEIGHT = screen.get_size()
    
    fonts = (
        pygame.font.SysFont(None, int(HEIGHT * 0.12)), 
        pygame.font.SysFont(None, int(HEIGHT * 0.08)), 
        pygame.font.SysFont(None, int(HEIGHT * 0.04))  
    )
    # Start the serial listener thread
    serial_thread = threading.Thread(target=serial_worker, daemon=True)
    serial_thread.start()


    # Load gifs/images for UI    
    background_gif = AnimatedGIF(GIF_FILENAME, WIDTH, HEIGHT, frame_delay=100)
    
    p_width = int(WIDTH * 0.225)
    p_height = int(HEIGHT * 0.4)
    
    test =  pygame.image.load("assets\\pixil-layer-Background.png").convert_alpha()
    test_image = pygame.transform.scale(test, (WIDTH * 0.8, p_height))
    
    
    player_2_gif = AnimatedGIF("assets\player 2 animation.gif", p_width, p_height, frame_delay=200)
    player_1_gif = AnimatedGIF("assets\player 1 animation.gif", p_width, p_height, frame_delay=200)
      
    ui_assets = {
    "test": test_image,
    "bg": background_gif,
    "p1_gif": player_1_gif,
    "p2_gif": player_2_gif
    }

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                running = False

        # Render Frame
        screen.fill((20, 20, 20))  # Clear baseline layout
        
        draw_ui(
            screen, fonts, game_data, calib_timers, 
            ui_assets,
            MAX_SCORE_DIFF
        )
        
        ##screen.fill(BLACK)
        
        pygame.display.flip()
        
        clock.tick(60) 

    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()