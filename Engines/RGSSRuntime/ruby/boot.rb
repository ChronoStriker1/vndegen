# Runs once, when Ruby starts, before any game. Ruby can start only once per process, so between games RGSSBoot.reset
# puts the interpreter back the way this snapshot found it: the game's constants, globals, threads and changes to
# Ruby's own classes go. A module a game included into a core class stays included, because Ruby can't remove it.
# Games redefine constants and methods as a matter of course; Ruby's warnings about it would only fill the log.
$VERBOSE = nil

module RGSSBoot
  CONSTANTS = Object.constants(false)
  GLOBALS = global_variables

  def self.method_table(mod)
    table = {}
    { public: mod.public_instance_methods(false), protected: mod.protected_instance_methods(false),
      private: mod.private_instance_methods(false) }.each do |visibility, names|
      names.each { |name| table[name] = [mod.instance_method(name), visibility] }
    end
    table
  end

  METHODS = {}
  ObjectSpace.each_object(Module) do |mod|
    next if mod.singleton_class? || mod.name.nil? || mod == RGSSBoot
    METHODS[mod] = [method_table(mod), method_table(mod.singleton_class)]
  end
  MAIN = TOPLEVEL_BINDING.receiver
  CLASS_VARIABLES = Object.class_variables
  @games = 0

  def self.restore(mod, saved)
    current = method_table(mod)
    current.each_key do |name|
      mod.send(:remove_method, name) unless saved.key?(name)
    rescue NameError
      nil
    end
    saved.each do |name, (method, visibility)|
      now = current[name]
      next if now && now[0] == method && now[1] == visibility
      mod.send(:define_method, name, method)
      mod.send(visibility, name)
    rescue StandardError
      nil
    end
  end

  def self.reset
    @games += 1
    return if @games == 1
    Thread.list.each { |thread| thread.kill unless thread == Thread.main }
    (Object.constants(false) - CONSTANTS).each do |name|
      Object.send(:remove_const, name)
    rescue NameError
      nil
    end
    (global_variables - GLOBALS).each do |name|
      eval("#{name} = nil")
    rescue StandardError, SyntaxError
      nil
    end
    (Object.class_variables - CLASS_VARIABLES).each { |name| Object.send(:remove_class_variable, name) }
    MAIN.instance_variables.each { |name| MAIN.remove_instance_variable(name) }
    METHODS.each do |mod, (instance, singleton)|
      restore(mod, instance)
      restore(mod.singleton_class, singleton)
    end
    GC.start
  end
end
