# VNDegen's cheats for RPG Maker XP, VX and VX Ace, on the game's own objects, so a save made afterwards is an ordinary
# save. VNDegen runs this file once the game's scripts have loaded, at the first frame after its Main section starts,
# and then calls the module the same way. The settings that stay on are modules prepended to the game's classes, which
# Marshal doesn't write into saves.
module VNDegenCheats
  @settings = {}
  @installed = false
  # Whether the game objects belong to a game the player started or loaded. VX and VX Ace make game objects for the
  # title screen, and starting or loading a game replaces them.
  @playing = false

  class << self
    def setting(name)
      @settings[name]
    end

    # True once the wraps are in place.
    def apply(settings)
      fill = settings['keepsPartyHealthy'] && !@settings['keepsPartyHealthy']
      @settings = settings
      install
      fill_party if fill && in_game?
      @installed
    end

    def install
      return if @installed || !defined?(Game_Actor) || !defined?(Game_Player) || !defined?(Scene_Map)
      @installed = true
      wrap_actors
      wrap_player
      wrap_scenes
    end

    def on_title
      @playing = false
    end

    def on_map
      @playing = true
      fill_party if setting('keepsPartyHealthy')
    end

    def in_game?
      scene = defined?(SceneManager) ? SceneManager.scene : $scene
      return false if defined?(Scene_Title) && scene.is_a?(Scene_Title)
      return false unless $game_party && $game_variables && $game_switches && $data_system
      @playing || scene.is_a?(Scene_Map)
    end

    # MARK: The party

    def party
      return [] unless $game_party
      [:all_members, :members, :actors].each { |name| return $game_party.send(name) if $game_party.respond_to?(name) }
      []
    end

    def max_hp(actor)
      actor.respond_to?(:mhp) ? actor.mhp : actor.maxhp
    end

    # XP calls MP SP.
    def mp(actor)
      actor.respond_to?(:mp) ? actor.mp : actor.sp
    end

    def max_mp(actor)
      return actor.mmp if actor.respond_to?(:mmp)
      actor.respond_to?(:maxmp) ? actor.maxmp : actor.maxsp
    end

    def fill_party
      party.each do |actor|
        actor.hp = max_hp(actor)
        if actor.respond_to?(:mp=)
          actor.mp = max_mp(actor)
        else
          actor.sp = max_mp(actor)
        end
      end
    end

    # MARK: Wraps

    def wrap_actors
      hp_limit = Game_Actor.method_defined?(:mhp) ? :mhp : :maxhp
      mp_setter = Game_Actor.method_defined?(:mp=) ? :mp= : :sp=
      mp_limit = Game_Actor.method_defined?(:mmp) ? :mmp : Game_Actor.method_defined?(:maxmp) ? :maxmp : :maxsp
      Game_Actor.prepend(Module.new do
        # Damage, skill costs and events all set HP and MP through these.
        define_method(:hp=) do |value|
          super(VNDegenCheats.setting('keepsPartyHealthy') ? [value, send(hp_limit)].max : value)
        end
        define_method(mp_setter) do |value|
          super(VNDegenCheats.setting('keepsPartyHealthy') ? [value, send(mp_limit)].max : value)
        end
        if Game_Actor.method_defined?(:final_exp_rate)
          # VX Ace gains experience at this rate.
          define_method(:final_exp_rate) { super() * VNDegenCheats.setting('experienceRate') }
        elsif Game_Actor.method_defined?(:gain_exp)
          # VX gains experience here.
          define_method(:gain_exp) { |exp, *rest| super(exp * VNDegenCheats.setting('experienceRate'), *rest) }
        else
          # XP's battles and events add experience through exp=.
          define_method(:exp=) do |value|
            gained = value - @exp
            super(gained > 0 ? @exp + gained * VNDegenCheats.setting('experienceRate') : value)
          end
        end
      end)
    end

    def wrap_player
      Game_Player.prepend(Module.new do
        if Game_Player.method_defined?(:encounter)
          # VX Ace starts a random battle when this is true.
          define_method(:encounter) { VNDegenCheats.setting('noEncounters') ? false : super() }
        else
          # XP's and VX's map scenes start one when this reaches 0.
          define_method(:encounter_count) do
            count = super()
            VNDegenCheats.setting('noEncounters') && count < 1 ? 1 : count
          end
        end
        if Game_Player.method_defined?(:debug_through?)
          # VX and VX Ace: what holding Control in a playtest does.
          define_method(:debug_through?) { VNDegenCheats.setting('walksThroughWalls') || super() }
        else
          # XP: anywhere on the map.
          define_method(:passable?) do |x, y, d|
            next super(x, y, d) unless VNDegenCheats.setting('walksThroughWalls')
            $game_map.valid?(x + (d == 6 ? 1 : d == 4 ? -1 : 0), y + (d == 2 ? 1 : d == 8 ? -1 : 0))
          end
        end
      end)
    end

    # XP runs a scene in main, VX and VX Ace start one in start.
    def wrap_scenes
      [[Scene_Map, :on_map], [defined?(Scene_Title) && Scene_Title, :on_title]].each do |scene, hook|
        next unless scene
        entry = scene.method_defined?(:start) ? :start : :main
        scene.prepend(Module.new do
          define_method(entry) do |*args|
            VNDegenCheats.send(hook)
            super(*args)
          end
        end)
      end
    end

    # MARK: State and changes

    def xp?
      $game_party.respond_to?(:gain_weapon)
    end

    def databases
      { 'item' => $data_items, 'weapon' => $data_weapons, 'armor' => $data_armors }
    end

    def owned(kind, entry)
      return $game_party.item_number(entry) unless xp?
      $game_party.send("#{kind}_number", entry.id)
    end

    def currency_unit
      return $data_system.currency_unit if $data_system.respond_to?(:currency_unit)
      return Vocab.gold if defined?(Vocab) && Vocab.respond_to?(:gold)
      $data_system.words.gold
    end

    # A variable's value: a whole number, text, or a description of anything else, which the page doesn't edit.
    def value(raw)
      case raw
      when Integer then raw.abs < 2**53 ? raw : { 'o' => raw.to_s }
      when String then { 's' => raw }
      when nil then 0
      else { 'o' => raw.inspect[0, 200] }
      end
    end

    # JSON, in the format VNCore's CheatState(json:) reads.
    def state
      return json('inGame' => false) unless in_game?
      items = []
      databases.each do |kind, list|
        (list || []).each do |entry|
          next unless entry && entry.name && !entry.name.empty?
          items << [kind, entry.id, entry.name, owned(kind, entry)]
        end
      end
      variables = (1...$data_system.variables.size).map { |id| [id, $data_system.variables[id] || '', value($game_variables[id])] }
      switches = (1...$data_system.switches.size).map { |id| [id, $data_system.switches[id] || '', !!$game_switches[id]] }
      party_state = party.map { |actor| [actor.name, actor.hp, max_hp(actor), mp(actor), max_mp(actor)] }
      json('inGame' => true, 'gold' => $game_party.gold, 'currencyUnit' => currency_unit, 'party' => party_state,
           'items' => items, 'variables' => variables, 'switches' => switches)
    end

    # False when no game is going, where the change would be lost.
    def perform(action)
      return false unless in_game?
      case action['type']
      when 'gold' then $game_party.gain_gold(action['amount'])
      when 'restore' then party.each(&:recover_all)
      when 'item'
        entry = (databases[action['kind']] || [])[action['id']]
        return false unless entry
        if xp?
          $game_party.send("gain_#{action['kind']}", entry.id, action['count'])
        else
          $game_party.gain_item(entry, action['count'])
        end
      when 'variable'
        $game_variables[action['id']] = action['value']
        $game_map.need_refresh = true if $game_map
      when 'switch'
        $game_switches[action['id']] = action['on']
        $game_map.need_refresh = true if $game_map
      else
        return false
      end
      true
    end

    def json(value)
      case value
      when String
        text = value.dup.force_encoding(Encoding::UTF_8).scrub
        '"' + text.gsub(/["\\\x00-\x1f]/) { |c| format('\u%04x', c.ord) } + '"'
      when Integer, true, false then value.to_s
      when nil then 'null'
      when Array then '[' + value.map { |v| json(v) }.join(',') + ']'
      when Hash then '{' + value.map { |k, v| json(k.to_s) + ':' + json(v) }.join(',') + '}'
      else json(value.to_s)
      end
    end
  end
end

VNDegenCheats.install
