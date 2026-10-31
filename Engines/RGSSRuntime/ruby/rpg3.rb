# The RPG module's methods for RPG Maker VX Ace (RGSS3), written from the behavior the help's RPG pages describe.
# rpg3_data.rb, loaded first, defines the data classes and their attributes.
module RPG
  class AudioFile
    def initialize(name = '', volume = 100, pitch = 100)
      @name = name
      @volume = volume
      @pitch = pitch
    end
  end

  # BGM and BGS remember what played last and where, so a scene can stop the music and resume it later.
  class BGM < AudioFile
    attr_accessor :pos

    def play(pos = 0)
      if @name.empty?
        Audio.bgm_stop
        @@last = BGM.new
      else
        Audio.bgm_play("Audio/BGM/#{@name}", @volume, @pitch, pos)
        @@last = clone
      end
    end

    def replay = play(@pos || 0)

    def self.stop
      Audio.bgm_stop
      @@last = BGM.new
    end

    def self.fade(time)
      Audio.bgm_fade(time)
      @@last = BGM.new
    end

    def self.last
      @@last.pos = Audio.bgm_pos
      @@last
    end

    @@last = BGM.new
  end

  class BGS < AudioFile
    attr_accessor :pos

    def play(pos = 0)
      if @name.empty?
        Audio.bgs_stop
        @@last = BGS.new
      else
        Audio.bgs_play("Audio/BGS/#{@name}", @volume, @pitch, pos)
        @@last = clone
      end
    end

    def replay = play(@pos || 0)

    def self.stop
      Audio.bgs_stop
      @@last = BGS.new
    end

    def self.fade(time)
      Audio.bgs_fade(time)
      @@last = BGS.new
    end

    def self.last
      @@last.pos = Audio.bgs_pos
      @@last
    end

    @@last = BGS.new
  end

  class ME < AudioFile
    def play
      if @name.empty? then Audio.me_stop
      else Audio.me_play("Audio/ME/#{@name}", @volume, @pitch)
      end
    end

    def self.stop = Audio.me_stop
    def self.fade(time) = Audio.me_fade(time)
  end

  class SE < AudioFile
    def play
      Audio.se_play("Audio/SE/#{@name}", @volume, @pitch) unless @name.empty?
    end

    def self.stop = Audio.se_stop
  end

  class Animation
    def to_screen? = @position == 3
  end

  class Weapon
    def performance = params[2] + params[4] + params.sum
  end

  class Armor
    def performance = params[3] + params[5] + params.sum
  end

  class Class
    # Total experience for a level from the class's four curve settings: base, extra, and the two acceleration terms.
    def exp_for_level(level)
      lv = level.to_f
      base, extra, accel_a, accel_b = @exp_params.map(&:to_f)
      (base * ((lv - 1) ** (0.9 + accel_a / 250)) * lv * (lv + 1) / (6 + lv ** 2 / 50 / accel_b) + (lv - 1) * extra).round.to_i
    end
  end

  class CommonEvent
    def autorun? = @trigger == 1
    def parallel? = @trigger == 2
  end

  class Item
    def key_item? = @itype_id == 2
  end

  class UsableItem
    def for_opponent? = [1, 2, 3, 4, 5, 6].include?(@scope)
    def for_friend? = [7, 8, 9, 10, 11].include?(@scope)
    def for_dead_friend? = [9, 10].include?(@scope)
    def for_user? = @scope == 11
    def for_one? = [1, 3, 7, 9, 11].include?(@scope)
    def for_random? = [3, 4, 5, 6].include?(@scope)
    def number_of_targets = for_random? ? @scope - 2 : 0
    def for_all? = [2, 8, 10].include?(@scope)
    def need_selection? = [1, 7, 9].include?(@scope)
    def battle_ok? = [0, 1].include?(@occasion)
    def menu_ok? = [0, 2].include?(@occasion)
    def certain? = @hit_type == 0
    def physical? = @hit_type == 1
    def magical? = @hit_type == 2

    class Damage
      def none? = @type == 0
      def to_hp? = [1, 3, 5].include?(@type)
      def to_mp? = [2, 4, 6].include?(@type)
      def recover? = [3, 4].include?(@type)
      def drain? = [5, 6].include?(@type)
      def sign = recover? ? -1 : 1

      # The damage formula from the database, evaluated with the user as `a`, the target as `b` and the game's
      # variables as `v`. A formula that fails counts as zero, as on PC.
      def eval(a, b, v)
        [Kernel.eval(@formula), 0].max * sign
      rescue StandardError
        0
      end
    end
  end
end
