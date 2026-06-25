import pygame
from PIL import Image, ImageSequence

class AnimatedGIF:
    def __init__(self, filename, width, height, frame_delay=100):
        """
        Initializes an independent animated GIF object.
        :param filename: Path to the GIF file.
        :param width: Target scale width.
        :param height: Target scale height.
        :param frame_delay: Animation speed in milliseconds per frame.
        """
        self.frame_delay = frame_delay
        self.last_update = pygame.time.get_ticks()
        self.current_frame = 0
        self.frames = []
        
        try:
            pil_image = Image.open(filename)
            for frame in ImageSequence.Iterator(pil_image):
                frame_rgba = frame.convert("RGBA")
                data = frame_rgba.tobytes()
                size = frame_rgba.size
                pygame_surface = pygame.image.fromstring(data, size, "RGBA")
                
                # Scale each individual frame to the requested width/height
                scaled_surface = pygame.transform.scale(pygame_surface, (width, height))
                self.frames.append(scaled_surface)
        except Exception as e:
            print(f"Error loading GIF '{filename}': {e}")
            # Fallback to a single transparent surface if loading fails
            fallback = pygame.Surface((width, height), pygame.SRCALPHA)
            self.frames = [fallback]

    def update(self):
        """Advances the internal animation frame tracking independent of other elements."""
        if not self.frames or len(self.frames) <= 1:
            return
            
        now = pygame.time.get_ticks()
        if now - self.last_update > self.frame_delay:
            self.current_frame = (self.current_frame + 1) % len(self.frames)
            self.last_update = now

    def draw(self, screen, position):
        """Draws the current frame onto the target screen at the given (x, y) coordinates."""
        if self.frames:
            screen.blit(self.frames[self.current_frame], position)