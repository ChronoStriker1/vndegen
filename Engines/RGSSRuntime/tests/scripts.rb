# Run with the runtime's Ruby: ruby Engines/RGSSRuntime/tests/scripts.rb
require 'zlib'
require_relative '../ruby/common'
require_relative '../ruby/rpg3_data'
require_relative '../ruby/rpg3'

def load_data(_path) = $test_sections

failures = []
check = lambda do |name, &test|
  test.call
  puts "PASS #{name}"
rescue Exception => error
  failures << name
  warn "FAIL #{name}: #{error.class}: #{error.message.to_s.scrub}"
end

[1, 2, 3].each do |version|
  check.call("RGSS#{version} loads a BOM before a block comment without moving lines") do
    $RGSS_VERSION = version
    $test_sections = [[0, 'Block comment', Zlib::Deflate.deflate("\uFEFF=begin\ncomment\n=end\n$test_script_line = __LINE__\n")]]
    $test_script_line = nil
    rgss_run_scripts('unused')
    raise 'script did not run on its original line' unless $test_script_line == 4
  end
end

check.call('a BOM inside a script string stays intact') do
  $test_sections = [[0, 'String', Zlib::Deflate.deflate("$test_script_text = \"\uFEFFtext\"\n")]]
  rgss_run_scripts('unused')
  raise 'embedded BOM changed' unless $test_script_text == "\uFEFFtext"
end

check.call('syntax errors with invalid UTF-8 name the section and line') do
  $RGSS_SCRIPTS = [[0, '日本語', '', '']]
  error = SyntaxError.new("{0000}:2: unexpected token \xFF".force_encoding(Encoding::UTF_8))
  description = rgss_describe_error(error)
  raise 'section or line missing' unless description.include?("Script '日本語' line 2: SyntaxError")
  raise 'error message missing' unless description.include?('unexpected token')
  raise 'description is not valid UTF-8' unless description.encoding == Encoding::UTF_8 && description.valid_encoding?
end

check.call('script records keep their ID, name, compressed bytes and runnable source') do
  source = "\uFEFF=begin\ncomment\n=end\n$test_script_line = __LINE__\n"
  compressed = Zlib::Deflate.deflate(source)
  $test_sections = [[42, '再実行', compressed]]
  rgss_run_scripts('unused')
  section = $RGSS_SCRIPTS.fetch(0)
  raise 'script record changed' unless section[0, 3] == [42, '再実行', compressed]
  $test_script_line = nil
  eval(section.fetch(3), TOPLEVEL_BINDING, '{0000}', 1)
  raise 'stored source did not run on its original line' unless $test_script_line == 4
end

check.call('VX Ace features accept defaults and game-supplied values') do
  feature = RPG::BaseItem::Feature.new
  raise 'feature defaults changed' unless [feature.code, feature.data_id, feature.value] == [0, 0, 0]
  value = { rate: 0.5 }
  feature = RPG::BaseItem::Feature.new(23, 4, value)
  raise 'feature arguments lost' unless [feature.code, feature.data_id, feature.value] == [23, 4, value]
end

check.call('VX Ace effects accept defaults and game-supplied values') do
  effect = RPG::UsableItem::Effect.new
  raise 'effect defaults changed' unless [effect.code, effect.data_id, effect.value1, effect.value2] == [0, 0, 0, 0]
  effect = RPG::UsableItem::Effect.new(11, 2, 0.5, 30)
  raise 'effect arguments lost' unless [effect.code, effect.data_id, effect.value1, effect.value2] == [11, 2, 0.5, 30]
end

check.call('VX Ace event and move commands retain their arguments') do
  command = RPG::EventCommand.new(101, 2, ['text'])
  raise 'event command arguments lost' unless [command.code, command.indent, command.parameters] == [101, 2, ['text']]
  move = RPG::MoveCommand.new(14, [3, 4])
  raise 'move command arguments lost' unless [move.code, move.parameters] == [14, [3, 4]]
end

check.call('runtime errors name their script section') do
  $test_sections = [[0, 'Runtime section', Zlib::Deflate.deflate("raise 'runtime detail'\n")]]
  begin
    rgss_run_scripts('unused')
  rescue RuntimeError => error
    description = rgss_describe_error(error)
    raise 'runtime section missing' unless description.include?("Script 'Runtime section' line 1: RuntimeError")
    raise 'runtime message missing' unless description.include?('runtime detail')
  else
    raise 'script did not raise'
  end
end

abort "#{failures.length} failed checks" unless failures.empty?
