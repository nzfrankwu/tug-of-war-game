import pygame
import time
from enum import IntEnum

# =======================================================================
# UI ENUMS & GRAPHICS CONSTANTS
# =======================================================================
class GameState(IntEnum):
    START_SCREEN = 0
    CALIBRATION = 1
    COUNTDOWN = 2
    PLAYING = 3
    GAME_OVER = 4

# UI Colors grouped here since they belong to the display layer
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

# =======================================================================
# UI RENDERING
# =======================================================================
def draw_ui(screen, fonts, game_data, calib_timers, ui_assets, MAX_SCORE_DIFF):
    # Extract variables by name
    test_image = ui_assets["test"]
    background_gif = ui_assets["bg"]
    player_1_gif = ui_assets["p1_gif"]
    player_2_gif = ui_assets["p2_gif"]
 
    
    
    
    WIDTH, HEIGHT = screen.get_size()
    state = game_data["game_state"]
    font_large, font_medium, font_small = fonts
    current_time = time.time()

    # Get actual hardware flexing states
    p1_flexing = game_data["p1_is_flexing"] == 1
    p2_flexing = game_data["p2_is_flexing"] == 1

    if game_data["p1_calib"] == 1 and calib_timers["p1_last_state"] == 0: calib_timers["p1_start_time"] = current_time
    calib_timers["p1_last_state"] = game_data["p1_calib"]

    if game_data["p2_calib"] == 1 and calib_timers["p2_last_state"] == 0: calib_timers["p2_start_time"] = current_time
    calib_timers["p2_last_state"] = game_data["p2_calib"]

    # ---------------------------------------------------------
    # SHARED BACKGROUND: PLAYERS & ROPE
    # ---------------------------------------------------------
    if state in [GameState.START_SCREEN, GameState.COUNTDOWN, GameState.PLAYING, GameState.GAME_OVER]:
        # NEW: Update and render this specific GIF background safely via class methods
        background_gif.update()
        background_gif.draw(screen, (0, 0))
        
        player_w = WIDTH * 0.225
        player_h = HEIGHT * 0.4
        
        # position of the center left edge of the left player image 
        leftplayer_pos_x = 0.1
        leftplayer_pos_y = 0.6
        
        screen.blit(test_image, (WIDTH * leftplayer_pos_x, HEIGHT * leftplayer_pos_y))
        
        # Player 1 Image Placement
        p1_rect = pygame.Rect(0, 0, player_w, player_h)
        p1_rect.midleft = (WIDTH * leftplayer_pos_x, HEIGHT * leftplayer_pos_y)
        if p1_flexing:
            pygame.draw.rect(screen, RED, p1_rect) 
        player_1_gif.update()  
        player_1_gif.draw(screen, p1_rect.topleft) 
        p1_label = font_small.render(f"P2: {game_data['p1_score']}", True, WHITE)
        screen.blit(p1_label, p1_label.get_rect(center=(p1_rect.centerx, p1_rect.bottom + HEIGHT * 0.05)))
    
        # Player 2 Box
        p2_rect = pygame.Rect(0, 0, player_w, player_h)
        p2_rect.midright = (WIDTH * (1 - leftplayer_pos_x), HEIGHT * leftplayer_pos_y)
        if p2_flexing:
            pygame.draw.rect(screen, RED, p2_rect)
        player_2_gif.update()
        player_2_gif.draw(screen, p2_rect.topleft)
        p2_label = font_small.render(f"P2: {game_data['p2_score']}", True, WHITE)
        screen.blit(p2_label, p2_label.get_rect(center=(p2_rect.centerx, p2_rect.bottom + HEIGHT * 0.05)))


        # Draw Rope & Flag
        rope_y = p2_rect.midleft[1]- (player_h /10) # Sligtly above the center of the player images to account for the where the rope was drawn
        rope_start_pos = (p1_rect.midright[0], rope_y)
        rope_end_pos = (p2_rect.midleft[0], rope_y)
        rope_thickness = max(5, int(player_w /18))
        pygame.draw.line(screen, ROPE_COLOR, rope_start_pos, rope_end_pos, rope_thickness)

        score_diff = game_data["p1_score"] - game_data["p2_score"]
        pull_percentage = score_diff / MAX_SCORE_DIFF
        pull_percentage = max(-1.0, min(1.0, pull_percentage)) 
        
        rope_center_x = WIDTH / 2
        
        # this now dynamically grabs the Y position of the rope and positions the flag and marker accordingly
        rope_y = rope_start_pos[1] 
        
        flag_w = WIDTH * 0.04
        padding = flag_w / 2
        max_travel = ((rope_end_pos[0] - rope_start_pos[0]) / 2) - padding
        
        flag_x = rope_center_x - (pull_percentage * max_travel)

        flag_h = HEIGHT * 0.08
        flag_rect = pygame.Rect(0, 0, flag_w, flag_h)
        flag_rect.center = (flag_x, rope_y)
        pygame.draw.rect(screen, (200, 0, 0), flag_rect)
        
        marker_h = HEIGHT * 0.1
        pygame.draw.line(screen, GRAY, (rope_center_x, rope_y - marker_h/2), (rope_center_x, rope_y + marker_h/2), 3)

    # ---------------------------------------------------------
    # STATE 0: START SCREEN
    # ---------------------------------------------------------
    if state == GameState.START_SCREEN:
        overlay = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 180)) 
        screen.blit(overlay, (0, 0))

        title = font_large.render("EMG TUG OF WAR", True, WHITE)
        screen.blit(title, title.get_rect(center=(WIDTH/2, HEIGHT * 0.3)))

        prompt = font_medium.render("Press START Button on Mainboard", True, YELLOW)
        if int(current_time * 2) % 2 == 0:
            screen.blit(prompt, prompt.get_rect(center=(WIDTH/2, HEIGHT * 0.6)))

    # ---------------------------------------------------------
    # STATE 1: CALIBRATION SCREEN
    # ---------------------------------------------------------
    elif state == GameState.CALIBRATION:
        title = font_large.render("CALIBRATION PHASE", True, YELLOW)
        screen.blit(title, title.get_rect(center=(WIDTH/2, HEIGHT * 0.15)))
        
        if game_data["p1_calib"] == 2: p1_color, p1_status = GREEN, "READY"
        elif game_data["p1_calib"] == 1: p1_color, p1_status = YELLOW, "CALIBRATING"
        else: p1_color, p1_status = RED, "WAITING"
            
        p1_text = font_medium.render(f"Player 1: {p1_status}", True, p1_color)
        screen.blit(p1_text, p1_text.get_rect(center=(WIDTH * 0.25, HEIGHT * 0.35)))
        
        if game_data["p2_calib"] == 2: p2_color, p2_status = GREEN, "READY"
        elif game_data["p2_calib"] == 1: p2_color, p2_status = YELLOW, "CALIBRATING"
        else: p2_color, p2_status = RED, "WAITING"
            
        p2_text = font_medium.render(f"Player 2: {p2_status}", True, p2_color)
        screen.blit(p2_text, p2_text.get_rect(center=(WIDTH * 0.75, HEIGHT * 0.35)))

        active_player = 0
        p1_elapsed = current_time - calib_timers["p1_start_time"] if calib_timers["p1_start_time"] > 0 else 0
        p2_elapsed = current_time - calib_timers["p2_start_time"] if calib_timers["p2_start_time"] > 0 else 0

        if game_data["p1_calib"] == 1: active_player, elapsed = 1, p1_elapsed
        elif game_data["p2_calib"] == 1: active_player, elapsed = 2, p2_elapsed

        if active_player == 0:
            inst1 = font_small.render("Press Mainboard Calibration Buttons to Begin", True, WHITE)
            screen.blit(inst1, inst1.get_rect(center=(WIDTH/2, HEIGHT * 0.65)))
        else:
            if elapsed <= 3.0: calib_phase, calib_progress = "RELAX", elapsed
            else: calib_phase, calib_progress = "FLEX", min(elapsed - 3.0, 3.0) 

            phase_text = "FLEX HARD!" if calib_phase == "FLEX" else "RELAX COMPLETELY..."
            phase_color = RED if calib_phase == "FLEX" else BLUE_BRIGHT
            
            p_text = font_medium.render(f"PLAYER {active_player} CALIBRATING...", True, WHITE)
            screen.blit(p_text, p_text.get_rect(center=(WIDTH/2, HEIGHT * 0.55)))
            
            action_text = font_large.render(phase_text, True, phase_color)
            screen.blit(action_text, action_text.get_rect(center=(WIDTH/2, HEIGHT * 0.7)))

            bar_w, bar_h = WIDTH * 0.4, HEIGHT * 0.05
            bar_rect = pygame.Rect(0, 0, bar_w, bar_h)
            bar_rect.center = (WIDTH/2, HEIGHT * 0.85)
            
            pygame.draw.rect(screen, GRAY, bar_rect, 3) 
            fill_w = max(0, min(bar_w, bar_w * (calib_progress / 3.0))) 
            fill_rect = pygame.Rect(bar_rect.left, bar_rect.top, fill_w, bar_h)
            pygame.draw.rect(screen, phase_color, fill_rect)
    # ---------------------------------------------------------
    # STATE 2: COUNTDOWN
    # ---------------------------------------------------------
    elif state == GameState.COUNTDOWN:
        # Track when we entered the countdown state to show 3, 2, 1
        if calib_timers.get("countdown_start", 0) == 0:
            calib_timers["countdown_start"] = current_time
            
        time_in_countdown = current_time - calib_timers["countdown_start"]
        
        # Draw the dark overlay
        overlay = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 180)) 
        screen.blit(overlay, (0, 0))
        
        # Determine the countdown number (0 to 3 seconds)
        if time_in_countdown < 1.0: 
            text_str = "3"
        elif time_in_countdown < 2.0: 
            text_str = "2"
        else: 
            text_str = "1"
            
        count_text = font_large.render(text_str, True, YELLOW)
        screen.blit(count_text, count_text.get_rect(center=(WIDTH/2, HEIGHT/2)))
        
    # ---------------------------------------------------------
    # STATE 3: PLAYING THE GAME
    # ---------------------------------------------------------
    elif state == GameState.PLAYING:
        box_rect = pygame.Rect(0, 0, WIDTH * 0.4, HEIGHT * 0.15)
        box_rect.center = (WIDTH / 2, HEIGHT * 0.15)
        
        if game_data["pattern_state"] == 1:
            pygame.draw.rect(screen, RED, box_rect, border_radius=15)
            prompt_text = font_large.render("FLEX NOW!", True, WHITE)
        else:
            pygame.draw.rect(screen, BLUE, box_rect, border_radius=15)
            prompt_text = font_large.render("RELAX...", True, WHITE)
            
        screen.blit(prompt_text, prompt_text.get_rect(center=box_rect.center))

    # ---------------------------------------------------------
    # STATE 4: GAME OVER
    # ---------------------------------------------------------
    elif state == GameState.GAME_OVER:
        overlay = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 180)) 
        screen.blit(overlay, (0, 0))

        winner = "PLAYER 1 WINS!" if game_data["p1_score"] > game_data["p2_score"] else "PLAYER 2 WINS!"
        if game_data["p1_score"] == game_data["p2_score"]: winner = "IT'S A TIE!"

        win_text = font_large.render(winner, True, GREEN)
        screen.blit(win_text, win_text.get_rect(center=(WIDTH/2, HEIGHT * 0.4)))
        
        prompt = font_medium.render("Press START Button to Play Again", True, WHITE)
        if int(current_time * 2) % 2 == 0:
            screen.blit(prompt, prompt.get_rect(center=(WIDTH/2, HEIGHT * 0.6)))






# p2_rect = pygame.Rect(0, 0, player_w, player_h)
        # p2_rect.midright = (WIDTH * (1-leftplayer_pos_x), HEIGHT *leftplayer_pos_y)
        # p2_color = BLUE_BRIGHT if p2_flexing else BLUE
        # pygame.draw.rect(screen, p2_color, p2_rect)
        # screen.blit(player_2_pull, p2_rect.topleft)
        # p2_label = font_small.render(f"P2: {game_data['p2_score']}", True, WHITE)
        # screen.blit(p2_label, p2_label.get_rect(center=(p2_rect.centerx, p2_rect.bottom + HEIGHT * 0.05)))