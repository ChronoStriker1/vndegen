# The RPG module's methods for RPG Maker VX (RGSS2), written from the behavior the help's RPG pages describe.
# rpg2_data.rb, loaded first, defines the data classes and their attributes.
module RPG
  class AudioFile
    def initialize(name = '', volume = 100, pitch = 100)
      @name = name
      @volume = volume
      @pitch = pitch
    end
  end

  # BGM and BGS remember what played last, so a scene can stop the music and resume it later.
  class BGM < AudioFile
    def play
      if @name.empty?
        Audio.bgm_stop
        @@last = BGM.new
      else
        Audio.bgm_play("Audio/BGM/#{@name}", @volume, @pitch)
        @@last = self
      end
    end

    def self.stop
      Audio.bgm_stop
      @@last = BGM.new
    end

    def self.fade(time)
      Audio.bgm_fade(time)
      @@last = BGM.new
    end

    def self.last = @@last

    @@last = BGM.new
  end

  class BGS < AudioFile
    def play
      if @name.empty?
        Audio.bgs_stop
        @@last = BGS.new
      else
        Audio.bgs_play("Audio/BGS/#{@name}", @volume, @pitch)
        @@last = self
      end
    end

    def self.stop
      Audio.bgs_stop
      @@last = BGS.new
    end

    def self.fade(time)
      Audio.bgs_fade(time)
      @@last = BGS.new
    end

    def self.last = @@last

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

  class Enemy
    class Action
      def skill? = @kind == 1
    end
  end

  class UsableItem
    def for_opponent? = [1, 2, 3, 4, 5, 6].include?(@scope)
    def for_friend? = [7, 8, 9, 10, 11].include?(@scope)
    def for_dead_friend? = [9, 10].include?(@scope)
    def for_user? = @scope == 11
    def for_one? = [1, 3, 4, 7, 9, 11].include?(@scope)
    def for_two? = @scope == 5
    def for_three? = @scope == 6
    def for_random? = [4, 5, 6].include?(@scope)
    def for_all? = [2, 8, 10].include?(@scope)
    def dual? = @scope == 3
    def need_selection? = [1, 3, 7, 9].include?(@scope)
    def battle_ok? = [0, 1].include?(@occasion)
    def menu_ok? = [0, 2].include?(@occasion)
  end
end
