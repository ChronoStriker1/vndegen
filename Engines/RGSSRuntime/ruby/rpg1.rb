# The RPG module's built-in classes and methods for RPG Maker XP (RGSS1), written from the behavior the help's RPG
# pages describe: RPG::Cache, RPG::Sprite and RPG::Weather. rpg1_data.rb, loaded first, defines the data classes.
module RPG
  class AudioFile
    def initialize(name = '', volume = 100, pitch = 100)
      @name = name
      @volume = volume
      @pitch = pitch
    end
  end

  # Loads each kind of graphic once and hands out the same Bitmap after that. An empty name gives a 32x32 bitmap, as
  # XP's "(None)" needs. A disposed bitmap is loaded again.
  module Cache
    @cache = {}

    def self.load_bitmap(folder, filename, hue = 0)
      path = folder + filename
      if !@cache.key?(path) || @cache[path].disposed?
        @cache[path] = filename.empty? ? Bitmap.new(32, 32) : Bitmap.new(path)
      end
      return @cache[path] if hue == 0
      key = [path, hue]
      if !@cache.key?(key) || @cache[key].disposed?
        @cache[key] = @cache[path].clone
        @cache[key].hue_change(hue)
      end
      @cache[key]
    end

    def self.animation(filename, hue) = load_bitmap('Graphics/Animations/', filename, hue)
    def self.autotile(filename) = load_bitmap('Graphics/Autotiles/', filename)
    def self.battleback(filename) = load_bitmap('Graphics/Battlebacks/', filename)
    def self.battler(filename, hue) = load_bitmap('Graphics/Battlers/', filename, hue)
    def self.character(filename, hue) = load_bitmap('Graphics/Characters/', filename, hue)
    def self.fog(filename, hue) = load_bitmap('Graphics/Fogs/', filename, hue)
    def self.gameover(filename) = load_bitmap('Graphics/Gameovers/', filename)
    def self.icon(filename) = load_bitmap('Graphics/Icons/', filename)
    def self.panorama(filename, hue) = load_bitmap('Graphics/Panoramas/', filename, hue)
    def self.picture(filename) = load_bitmap('Graphics/Pictures/', filename)
    def self.tileset(filename) = load_bitmap('Graphics/Tilesets/', filename)
    def self.title(filename) = load_bitmap('Graphics/Titles/', filename)
    def self.windowskin(filename) = load_bitmap('Graphics/Windowskins/', filename)

    # One 32x32 tile of a tileset, for an event whose graphic is a tile. Tile IDs from 384 count tileset tiles.
    def self.tile(filename, tile_id, hue)
      key = [filename, tile_id, hue]
      if !@cache.key?(key) || @cache[key].disposed?
        bitmap = Bitmap.new(32, 32)
        index = tile_id - 384
        bitmap.blt(0, 0, tileset(filename), Rect.new(index % 8 * 32, index / 8 * 32, 32, 32))
        bitmap.hue_change(hue)
        @cache[key] = bitmap
      end
      @cache[key]
    end

    def self.clear
      @cache = {}
      GC.start
    end
  end

  # A Sprite with the effects XP's battle and map scenes use. Animations and damage figures are drawn by sprites of
  # their own at z 2000 and 3000.
  class Sprite < ::Sprite
    # Animation graphics in use, counted, so one shared by several sprites is freed only after the last.
    @@animation_users = Hash.new(0)
    # Screen-wide animations already started this frame, so several targets show one.
    @@screen_animations = []

    def initialize(viewport = nil)
      super(viewport)
      @whiten_duration = @appear_duration = @escape_duration = @collapse_duration = 0
      @damage_duration = @animation_duration = 0
      @blink = false
      @blink_count = 0
    end

    def dispose
      dispose_damage
      dispose_animation
      dispose_loop_animation
      super
    end

    def whiten
      self.blend_type = 0
      self.color.set(255, 255, 255, 128)
      self.opacity = 255
      @whiten_duration = 16
      @appear_duration = @escape_duration = @collapse_duration = 0
    end

    def appear
      self.blend_type = 0
      self.color.set(0, 0, 0, 0)
      self.opacity = 0
      @appear_duration = 16
      @whiten_duration = @escape_duration = @collapse_duration = 0
    end

    def escape
      self.blend_type = 0
      self.color.set(0, 0, 0, 0)
      self.opacity = 255
      @escape_duration = 32
      @whiten_duration = @appear_duration = @collapse_duration = 0
    end

    def collapse
      self.blend_type = 1
      self.color.set(255, 64, 64, 255)
      self.opacity = 255
      @collapse_duration = 48
      @whiten_duration = @appear_duration = @escape_duration = 0
    end

    # A pop-up of the damage figure, white for damage, green for recovery, as is for text.
    def damage(value, critical)
      dispose_damage
      text = value.is_a?(Numeric) ? value.abs.to_s : value.to_s
      bitmap = Bitmap.new(160, 48)
      bitmap.font.name = 'Arial Black'
      bitmap.font.size = 32
      bitmap.font.color.set(0, 0, 0)
      [[-1, 11], [1, 11], [-1, 13], [1, 13]].each { |dx, dy| bitmap.draw_text(dx, 12 + dy - 12, 160, 36, text, 1) }
      if value.is_a?(Numeric) && value < 0
        bitmap.font.color.set(176, 255, 144)
      else
        bitmap.font.color.set(255, 255, 255)
      end
      bitmap.draw_text(0, 12, 160, 36, text, 1)
      if critical
        bitmap.font.size = 20
        bitmap.font.color.set(0, 0, 0)
        bitmap.draw_text(-1, -1, 160, 20, 'CRITICAL', 1)
        bitmap.draw_text(1, 1, 160, 20, 'CRITICAL', 1)
        bitmap.font.color.set(255, 255, 255)
        bitmap.draw_text(0, 0, 160, 20, 'CRITICAL', 1)
      end
      @damage_sprite = ::Sprite.new(viewport)
      @damage_sprite.bitmap = bitmap
      @damage_sprite.ox = 80
      @damage_sprite.oy = 20
      @damage_sprite.x = x
      @damage_sprite.y = y - oy / 2
      @damage_sprite.z = 3000
      @damage_duration = 40
    end

    def animation(animation, hit)
      dispose_animation
      @animation = animation
      return if @animation.nil?
      @animation_hit = hit
      @animation_duration = @animation.frame_max
      @animation_sprites = make_animation_sprites(@animation)
      @animation_bitmap = @animation_sprites.first&.bitmap
      update_animation_frame(@animation, 0, @animation_sprites, hit)
    end

    def loop_animation(animation)
      return if animation == @loop_animation
      dispose_loop_animation
      @loop_animation = animation
      return if @loop_animation.nil?
      @loop_animation_index = 0
      @loop_animation_sprites = make_animation_sprites(@loop_animation)
      update_animation_frame(@loop_animation, 0, @loop_animation_sprites, true)
    end

    def blink_on
      return if @blink
      @blink = true
      @blink_count = 0
    end

    def blink_off
      return unless @blink
      @blink = false
      self.color.set(0, 0, 0, 0)
    end

    def blink? = @blink

    def effect?
      @whiten_duration > 0 || @appear_duration > 0 || @escape_duration > 0 || @collapse_duration > 0 ||
        @damage_duration > 0 || @animation_duration > 0
    end

    def update
      super
      if @whiten_duration > 0
        @whiten_duration -= 1
        self.color.alpha = 128 - (16 - @whiten_duration) * 10
      end
      if @appear_duration > 0
        @appear_duration -= 1
        self.opacity = (16 - @appear_duration) * 16
      end
      if @escape_duration > 0
        @escape_duration -= 1
        self.opacity = 256 - (32 - @escape_duration) * 10
      end
      if @collapse_duration > 0
        @collapse_duration -= 1
        self.opacity = 256 - (48 - @collapse_duration) * 6
      end
      update_damage
      # XP's animations run at half the frame rate.
      if @animation && Graphics.frame_count % 2 == 0
        @animation_duration -= 1
        update_animation_frame(@animation, @animation.frame_max - @animation_duration, @animation_sprites, @animation_hit)
        dispose_animation if @animation_duration <= 0
      end
      if @loop_animation && Graphics.frame_count % 2 == 0
        @loop_animation_index = (@loop_animation_index + 1) % @loop_animation.frame_max
        update_animation_frame(@loop_animation, @loop_animation_index, @loop_animation_sprites, true)
      end
      if @blink
        @blink_count = (@blink_count + 1) % 32
        alpha = @blink_count < 16 ? (16 - @blink_count) * 6 : (@blink_count - 16) * 6
        self.color.set(255, 255, 255, alpha)
      end
      @@screen_animations.clear
    end

    def x=(value)
      offset = value - x
      move_effects(offset, 0)
      super
    end

    def y=(value)
      offset = value - y
      move_effects(0, offset)
      super
    end

    private

    def move_effects(dx, dy)
      return if dx == 0 && dy == 0
      [@animation_sprites, @loop_animation_sprites].compact.flatten.each do |sprite|
        sprite.x += dx
        sprite.y += dy
      end
    end

    def update_damage
      return unless @damage_duration > 0
      @damage_duration -= 1
      case @damage_duration
      when 38..39 then @damage_sprite.y -= 4
      when 36..37 then @damage_sprite.y -= 2
      when 34..35 then @damage_sprite.y += 2
      when 28..33 then @damage_sprite.y += 4
      end
      @damage_sprite.opacity = 256 - (12 - @damage_duration) * 32
      dispose_damage if @damage_duration == 0
    end

    def dispose_damage
      return unless @damage_sprite
      @damage_sprite.bitmap.dispose
      @damage_sprite.dispose
      @damage_sprite = nil
      @damage_duration = 0
    end

    def make_animation_sprites(animation)
      bitmap = RPG::Cache.animation(animation.animation_name, animation.animation_hue)
      @@animation_users[bitmap] += 1
      if animation.position == 3 && @@screen_animations.include?(animation)
        return []
      end
      @@screen_animations << animation if animation.position == 3
      Array.new(16) do
        sprite = ::Sprite.new(viewport)
        sprite.bitmap = bitmap
        sprite.visible = false
        sprite
      end
    end

    def release_animation_sprites(sprites)
      return unless sprites
      bitmap = sprites.first&.bitmap
      sprites.each(&:dispose)
      return unless bitmap
      @@animation_users[bitmap] -= 1
      if @@animation_users[bitmap] <= 0
        @@animation_users.delete(bitmap)
        bitmap.dispose
      end
    end

    def dispose_animation
      release_animation_sprites(@animation_sprites)
      @animation_sprites = nil
      @animation = nil
      @animation_duration = 0
    end

    def dispose_loop_animation
      release_animation_sprites(@loop_animation_sprites)
      @loop_animation_sprites = nil
      @loop_animation = nil
    end

    # Places the frame's cells. Each cell row of the frame's Table holds pattern (-1 for none), x, y, zoom percent,
    # angle, mirror, opacity and blend type; patterns are 192x192 squares, five to a row of the graphic.
    def update_animation_frame(animation, index, sprites, hit)
      return if sprites.nil? || sprites.empty?
      frame = animation.frames[index]
      if frame
        cells = frame.cell_data
        center_x, center_y = animation_center(animation)
        sprites.each_with_index do |sprite, i|
          pattern = i < frame.cell_max ? cells[i, 0] : nil
          if pattern.nil? || pattern == -1
            sprite.visible = false
            next
          end
          sprite.visible = true
          sprite.src_rect.set(pattern % 5 * 192, pattern / 5 * 192, 192, 192)
          sprite.x = center_x + cells[i, 1]
          sprite.y = center_y + cells[i, 2]
          sprite.z = 2000
          sprite.ox = sprite.oy = 96
          sprite.zoom_x = sprite.zoom_y = cells[i, 3] / 100.0
          sprite.angle = cells[i, 4]
          sprite.mirror = cells[i, 5] == 1
          sprite.opacity = cells[i, 6] * opacity / 255.0
          sprite.blend_type = cells[i, 7]
        end
      end
      animation.timings.each do |timing|
        next unless timing.frame == index
        next unless timing.condition == 0 || (timing.condition == 1 && hit) || (timing.condition == 2 && !hit)
        Audio.se_play('Audio/SE/' + timing.se.name, timing.se.volume, timing.se.pitch) unless timing.se.name.to_s.empty?
        case timing.flash_scope
        when 1 then flash(timing.flash_color, timing.flash_duration * 2)
        when 2 then viewport&.flash(timing.flash_color, timing.flash_duration * 2)
        when 3 then flash(nil, timing.flash_duration * 2)
        end
      end
    end

    # Where an animation centers: on the sprite's head, middle or feet, or the middle of the screen.
    def animation_center(animation)
      return [320, 240] if animation.position == 3
      height = src_rect.height
      case animation.position
      when 0 then [x - ox + src_rect.width / 2, y - oy + height / 4]
      when 1 then [x - ox + src_rect.width / 2, y - oy + height / 2]
      else [x - ox + src_rect.width / 2, y - oy + height]
      end
    end
  end

  # Rain, storm and snow: up to 40 sprites falling across the screen, scrolling with the map.
  class Weather
    attr_reader :type, :max, :ox, :oy

    def initialize(viewport = nil)
      @type = 0
      @max = 0
      @ox = 0
      @oy = 0
      color1 = Color.new(255, 255, 255, 255)
      color2 = Color.new(255, 255, 255, 128)
      @rain_bitmap = Bitmap.new(7, 56)
      7.times { |i| @rain_bitmap.fill_rect(6 - i, i * 8, 1, 8, color1) }
      @storm_bitmap = Bitmap.new(34, 64)
      32.times do |i|
        @storm_bitmap.fill_rect(33 - i, i * 2, 1, 2, color2)
        @storm_bitmap.fill_rect(32 - i, i * 2, 1, 2, color1)
        @storm_bitmap.fill_rect(31 - i, i * 2, 1, 2, color2)
      end
      @snow_bitmap = Bitmap.new(6, 6)
      @snow_bitmap.fill_rect(0, 1, 6, 4, color2)
      @snow_bitmap.fill_rect(1, 0, 4, 6, color2)
      @snow_bitmap.fill_rect(1, 2, 4, 2, color1)
      @snow_bitmap.fill_rect(2, 1, 2, 4, color1)
      @sprites = Array.new(40) do
        sprite = ::Sprite.new(viewport)
        sprite.z = 1000
        sprite.visible = false
        sprite.opacity = 0
        sprite
      end
    end

    def dispose
      @sprites.each(&:dispose)
      [@rain_bitmap, @storm_bitmap, @snow_bitmap].each(&:dispose)
    end

    def type=(type)
      return if @type == type
      @type = type
      bitmap = [nil, @rain_bitmap, @storm_bitmap, @snow_bitmap][type]
      @sprites.each_with_index do |sprite, i|
        sprite.visible = i < @max && !bitmap.nil?
        sprite.bitmap = bitmap
      end
    end

    def max=(max)
      @max = max.clamp(0, 40)
      @sprites.each_with_index { |sprite, i| sprite.visible = i < @max && @type != 0 }
    end

    def ox=(ox)
      return if @ox == ox
      @sprites.each { |sprite| sprite.x -= ox - @ox }
      @ox = ox
    end

    def oy=(oy)
      return if @oy == oy
      @sprites.each { |sprite| sprite.y -= oy - @oy }
      @oy = oy
    end

    def update
      return if @type == 0
      @max.times do |i|
        sprite = @sprites[i]
        case @type
        when 1
          sprite.x -= 2
          sprite.y += 16
          sprite.opacity -= 8
        when 2
          sprite.x -= 8
          sprite.y += 16
          sprite.opacity -= 12
        when 3
          sprite.x -= 2
          sprite.y += 8
          sprite.opacity -= 8
        end
        x = sprite.x - @ox
        y = sprite.y - @oy
        next unless sprite.opacity < 64 || x < -50 || x > 750 || y < -300 || y > 500
        sprite.x = rand(800) - 50 + @ox
        sprite.y = rand(800) - 200 + @oy
        sprite.opacity = 255
      end
    end
  end
end
