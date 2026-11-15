require 'fileutils'
require 'open3'
require 'tmpdir'
require 'zlib'

rgssrun = ARGV[0] or abort 'usage: ruby transition.rb path/to/rgssrun'

GAME = <<~'RUBY'
  W = Graphics.width
  H = Graphics.height
  $bitmap = Bitmap.new(W, H)
  $sprite = Sprite.new
  $sprite.bitmap = $bitmap
  def paint(r, g, b) = $bitmap.fill_rect(0, 0, W, H, Color.new(r, g, b))
  def hold(n) = n.times { Graphics.update }
  $marks = []
  def mark(name) = $marks << "#{name}=#{Graphics.brightness}=#{Graphics.frame_count}"

  def red
    Graphics.brightness = 255
    paint(255, 0, 0)
    hold(5)
  end

  paint(255, 0, 0)
  hold(10)
  Graphics.fadeout(30)
  mark 'faded'
  Graphics.freeze
  mark 'frozen'
  paint(0, 0, 255)
  Graphics.transition(20)
  mark 'transitioned'
  hold(5)
  mark 'shown'

  red
  Graphics.brightness = 128
  hold(5)
  mark 'dimmed'
  Graphics.freeze
  paint(0, 0, 255)
  Graphics.transition(20)
  mark 'transitioned from dim'

  red
  Graphics.freeze
  mark 'frozen bright'
  Graphics.brightness = 0
  hold(5)
  Graphics.brightness = 128
  hold(5)
  Graphics.fadeout(10)
  mark 'darkened while frozen'
  paint(0, 0, 255)
  Graphics.transition(20)
  mark 'transitioned from bright'

  red
  Graphics.fadeout(10)
  paint(0, 0, 255)
  Graphics.transition(10)
  mark 'transition without freeze'
  hold(5)
  mark 'still dark'

  red
  Graphics.fadeout(10)
  Graphics.freeze
  paint(0, 0, 255)
  Graphics.transition(0)
  mark 'cut'
  hold(5)
  mark 'end'

  $snaps = []
  def snap(name)
    bitmap = Graphics.snap_to_bitmap
    c = bitmap.get_pixel(W / 2, H / 2)
    $snaps << "#{name}=#{[c.red, c.green, c.blue, c.alpha].map(&:to_i).join(',')}"
  end
  paint(200, 100, 50)
  [255, 128, 10, 0].each do |v|
    Graphics.brightness = v
    snap "b#{v}"
  end
  Graphics.brightness = 255
  Graphics.freeze
  paint(0, 255, 0)
  Graphics.brightness = 64
  snap 'frozen'
  text = "#{$marks.join(';')}|#{$snaps.join(';')}"
  $RGSS_VERSION == 3 ? msgbox(text) : print(text)
  exit
RUBY

def run_game(rgssrun, version)
  Dir.mktmpdir do |dir|
    scripts = version == 3 ? 'Data/Scripts.rvdata2' : 'Data/Scripts.rvdata'
    FileUtils.mkdir_p(File.join(dir, 'Data'))
    File.write(File.join(dir, 'Game.ini'), "[Game]\r\nRTP=\r\nScripts=#{scripts.tr('/', '\\')}\r\nTitle=transition\r\n")
    code = "$RGSS_VERSION = #{version}\n#{GAME}".b
    File.binwrite(File.join(dir, scripts), Marshal.dump([[1, 'transition'.b, Zlib::Deflate.deflate(code).b]]))
    out, status = Open3.capture2e(rgssrun, '--ini', File.join(dir, 'Game.ini'), '--save', File.join(dir, 'save'),
                                  '--frames', '400', '--pixel', '100,100')
    [out, status]
  end
end

def results(out)
  pixels = out.scan(/^pixel \d+: (\d+) (\d+) (\d+)$/).map { |p| p.map(&:to_i) }
  message = out[/^msgbox \d+: (.*)$/, 1].to_s
  marks = message.split('|').first.to_s.split(';').map { |m| m.split('=') }
  snaps = message.split('|')[1].to_s.split(';').to_h { |m| m.split('=') }
  shift = pixels.length - marks.last[2].to_i
  start = 0
  segments = marks.to_h do |name, brightness, frame|
    before = pixels[[start + shift, 0].max...(frame.to_i + shift)]
    start = frame.to_i
    [name, {brightness: brightness.to_i, before: before}]
  end
  [segments, snaps]
end

failures = []
[2, 3].each do |version|
  out, status = run_game(rgssrun, version)
  s, snaps = results(out)
  check = lambda do |name, &test|
    test.call
    puts "PASS RGSS#{version} #{name}"
  rescue StandardError => error
    failures << "RGSS#{version} #{name}"
    warn "FAIL RGSS#{version} #{name}: #{error.message}"
  end
  near = ->(a, b, by = 3) { a.zip(b).all? { |x, y| (x - y).abs <= by } }
  frames_before = ->(name, n) do
    frames = s[name][:before]
    raise "#{name}: #{frames.length} frames, want #{n}" unless frames.length == n
    frames
  end
  brightness = ->(name, want) do
    raise "#{name} missing" unless s[name]
    raise "#{name}: brightness #{s[name][:brightness]}, want #{want}" unless s[name][:brightness] == want
  end

  check.call('the game ran to its end') do
    raise "rgssrun failed:\n#{out}" unless status.success? && s['end']
  end
  check.call('fadeout leaves brightness at 0, and freeze keeps it') do
    brightness.('faded', 0)
    brightness.('frozen', 0)
  end
  check.call('a transition after a freeze fades in from the black screen and leaves brightness at 255') do
    brightness.('transitioned', 255)
    frames = frames_before.('transitioned', 20)
    raise "first frame #{frames.first}, want near black" unless near.(frames.first, [0, 0, 13], 6)
    raise "frames leave the blue: #{frames}" unless frames.all? { |r, g, _| r.zero? && g.zero? }
    raise "blue doesn't rise: #{frames.map(&:last)}" unless frames.map(&:last).each_cons(2).all? { |a, b| b > a }
    raise "last frame #{frames.last}, want blue" unless frames.last == [0, 0, 255]
    raise "after: #{s['shown'][:before]}" unless frames_before.('shown', 5) == [[0, 0, 255]] * 5
  end
  check.call('a frozen dim screen blends as it was shown') do
    brightness.('dimmed', 128)
    brightness.('transitioned from dim', 255)
    frames = frames_before.('transitioned from dim', 20)
    raise "first frame #{frames.first}, want dim red" unless near.(frames.first, [121, 0, 13], 6)
    raise "last frame #{frames.last}, want blue" unless frames.last == [0, 0, 255]
  end
  check.call('brightness changes while frozen leave the frozen picture as it was') do
    brightness.('darkened while frozen', 0)
    held = frames_before.('darkened while frozen', 20)
    raise "held frames #{held.uniq}, want red" unless held.all? { |p| p == [255, 0, 0] }
    brightness.('transitioned from bright', 255)
    frames = frames_before.('transitioned from bright', 20)
    raise "first frame #{frames.first}, want red" unless near.(frames.first, [242, 0, 13], 6)
    raise "last frame #{frames.last}, want blue" unless frames.last == [0, 0, 255]
  end
  check.call('a transition without a freeze does nothing') do
    brightness.('transition without freeze', 0)
    raise "faded: #{s['transition without freeze'][:before]}" unless frames_before.('transition without freeze', 15).last == [0, 0, 0]
    raise "after: #{s['still dark'][:before]}" unless frames_before.('still dark', 5) == [[0, 0, 0]] * 5
  end
  check.call('a transition of no frames restores brightness at once') do
    brightness.('cut', 255)
    raise "after: #{s['end'][:before]}" unless frames_before.('end', 5) == [[0, 0, 255]] * 5
  end
  check.call('snap_to_bitmap draws the scene at the current brightness, and nothing at 0') do
    want = {'b255' => '200,100,50,255', 'b128' => '100,50,25,255', 'b10' => '7,3,1,255', 'b0' => '0,0,0,0',
            'frozen' => '0,63,0,255'}
    raise "snaps #{snaps}, want #{want}" unless snaps == want
  end
end

abort "#{failures.length} failed checks" unless failures.empty?
