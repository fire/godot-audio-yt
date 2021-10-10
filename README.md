# gdaudioyt

### [Godot Engine](https://godotengine.org) module for [YouTube](https://www.youtube.com) audio playback.

### Usage

```py
# Example usage of gdaudioyt in Godot Engine.
# https://github.com/nathanfranke/gdaudioyt/
extends Node

# There should be an AudioStreamPlayer child of this node.
onready var player: AudioStreamPlayer = $AudioStreamPlayer
# Reference to audio stream.
var stream := AudioStreamYT.new()

func _ready() -> void:
	# Get the YouTube video info.
	# Note: This is not required to play the audio.
	var info: VideoData = yield(YouTube.get_video("QPKKQnijnsM"), "completed")
	print(info.channel, ": ", info.title)

	# Set the YouTube ID.
	stream.create("QPKKQnijnsM")

	# Assign the stream and play it.
	player.stream = stream
	player.play()

# This part is optional, but useful to show the module's capabilities.
func _process(_delta: float) -> void:
	# Seek left and right 5 seconds.
	if Input.is_action_just_pressed("ui_left"):
		player.seek(player.get_playback_position() - 5.0)
	if Input.is_action_just_pressed("ui_right"):
		player.seek(player.get_playback_position() + 5.0)

	# Print current time and duration.
	print("%.3f/%.3f" % [player.get_playback_position(), stream.get_length()])

```

### Thanks

* Webm parsing highly utilizes [libmkv](https://github.com/quadrifoglio/libmkv), which generously uses a permissive license.
