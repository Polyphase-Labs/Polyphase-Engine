# Frequently Asked Questions


# Black Screen - Built Game

## Log Files
1. Go to `Edit > App Settings > Runtime > Log To File` and enable it.
2. Build your game again and run it.
3. After the game crashes, go to the `./Polyphase.log` file in your game's directory and open it with a text editor.
4. Look for any error messages or warnings that might indicate the cause of the black screen.

## 	Check Project Directory Structure vs Project Name
You project directory that the `{ProjectName}.oct` file is in must be named the same as the project name. For example, if your project is named "MyGame", the directory should be named "MyGame" and contain the `MyGame.oct` file. If there is a mismatch, the game will not load at all after a successful build.