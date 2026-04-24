#include "raylib.h"
#include "siwlib.h"
#include "constants.h"
#include "network.h"
#include "message_types.h"
#include <cstring>
#include <string>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <map>

// VIBE CODED MESSAGE READ AND WRITE FUNCTIONS
string float_to_hex(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    ostringstream oss;
    oss << std::hex << setw(8) << setfill('0') << bits;
    return oss.str();
}
float hex_to_float(const string& s)
{
    uint32_t bits;
    istringstream iss(s);
    iss >> std::hex >> bits;
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
string int_to_hex(int value)
{
    ostringstream oss;
    oss << std::setw(10) << std::setfill('0') << value;
    return oss.str();
}
int hex_to_int(const string& s)
{
    return std::stoi(s);
}

struct Cursor
{
	Vector2 pos;
	Color col;
};

static void AddCodepointRange(Font *font, const char *fontPath, int start, int stop)
{
    int rangeSize = stop - start + 1;
    int currentRangeSize = font->glyphCount;

    // TODO: Load glyphs from provided vector font (if available),
    // add them to existing font, regenerating font image and texture

    int updatedCodepointCount = currentRangeSize + rangeSize;
    int *updatedCodepoints = (int *)RL_CALLOC(updatedCodepointCount, sizeof(int));

    // Get current codepoint list
    for (int i = 0; i < currentRangeSize; i++) updatedCodepoints[i] = font->glyphs[i].value;

    // Add new codepoints to list (provided range)
    for (int i = currentRangeSize; i < updatedCodepointCount; i++)
        updatedCodepoints[i] = start + (i - currentRangeSize);

    UnloadFont(*font);
    *font = LoadFontEx(fontPath, 32, updatedCodepoints, updatedCodepointCount);
    RL_FREE(updatedCodepoints);
}


int main()
{
	if (!init_network("127.0.0.1", 12345)){
        TraceLog(LOG_FATAL, "The server is not running, please run server.exe");
        CloseWindow();
        return 1;
    }

	// RAYLIB INITIALIZATION
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
	SetTraceLogLevel(LOG_ALL);
	SetTargetFPS(60);
	InitWindow(800, 600, "doska");
	MaximizeWindow();
	InitAudioDevice();

	unsigned long long frame = 0;

	// CAMERA
	Camera2D cam;
	cam.offset = Vector2{GetScreenWidth() / 2, GetScreenHeight() / 2};
	cam.target = {0};
	cam.zoom = 1;
	cam.rotation = 0;

	Font font = LoadFont("resources/calibri_bold.ttf");
    AddCodepointRange(&font, "resources/calibri_bold.ttf", 0x400, 0x4ff);
    AddCodepointRange(&font, "resources/calibri_bold.ttf", 0x500, 0x52f);
    AddCodepointRange(&font, "resources/calibri_bold.ttf", 0x2de0, 0x2Dff);
    AddCodepointRange(&font, "resources/calibri_bold.ttf", 0xa640, 0xA69f);

	// SIDE BAR
	Rectangle side_bar, side_bar_open, color_pick;
	side_bar.x = -100;
	bool side_bar_opened = 1;
	int mode = 1;
	int buttons = 8;
	vector <float> button_size(buttons);
	vector <float> button_target_size(buttons);
	button_size[0] = BUTTON_SELECTED_SIZE;
	button_target_size[0] = BUTTON_SELECTED_SIZE;
	for (int i = 1; i < buttons; i++){
		button_size[i] = BUTTON_BASE_SIZE;
		button_target_size[i] = BUTTON_BASE_SIZE;
	}
	vector <Texture2D> button_texture = {LoadTexture("resources/mode1.png"), LoadTexture("resources/mode2.png"), LoadTexture("resources/mode3.png"), LoadTexture("resources/mode4.png"), LoadTexture("resources/mode5.png"), LoadTexture("resources/mode6.png"), LoadTexture("resources/mode7.png"), LoadTexture("resources/mode8.png")};
	Texture2D Open = LoadTexture("resources/open.png");
	Texture2D Close = LoadTexture("resources/close.png");
	Sound OpenSnd = LoadSound("resources/open.mp3");
	Sound CloseSnd = LoadSound("resources/close.mp3");
	Sound ButtonSnd = LoadSound("resources/button.mp3");
	int button_selected = -1;
	float color_anim = -400;
	int color_selected = -1;

	vector <int> action;
	vector <int> by_user;
    vector <bool> deleted;

	// DRAWING
	bool drawing = false;
	vector <Vector2> draw_traj;
	vector <vector<Vector2>> draws;
	long long draw_start;
	Color draw_color = BLACK;
	vector <Color> draw_colors;
	float draw_thick = 10;
	vector <float> draw_thicks;
	vector <int> modes;

	// LINES
	bool line = false;
	Vector2 line_start;
	Vector2 line_end;

	// RECTANGLE
	bool rec = false;
	Vector2 rec_start;
	Vector2 rec_end;

	// ELLIPSE
	bool elip = false;
	Vector2 elip_start;
	Vector2 elip_end;

	// TEXT
	bool text = false;
	Vector2 text_pos;
	string text_text;
	vector <Vector2> texts;
	vector <string> text_texts;
	vector <Color> text_colors;
	vector <float> text_sizes;

	// HAND
	bool hand = false;
	Vector2 grab_start;
	Vector2 prev_target = cam.target;

    // ONLINE
    int users = -1;
    int am_user = 0;
    bool am_admin = 0;
	bool sending_to_new = 0;
	int send_to_new_idx = 0;
	vector <int> new_users;
	vector <string> message_history;
	map <int, Cursor> cursors;
	Texture2D Cursorspr = LoadTexture("resources/cursor.png");
    
    TraceLog(LOG_INFO, "Joined");

	// GET USERS AMOUNT
    while (users == -1){
        while (has_message()){
            string msg = get_message();
            TraceLog(LOG_INFO, "Received: %s", msg.c_str());
            if (msg[0] == MSG_SEND_USERS_AMOUNT){
                users = hex_to_int(msg.substr(1, 10));
                am_user = hex_to_int(msg.substr(11, 10));
                am_admin = (users == 1);
                break;
            }
        }
    }
    
    TraceLog(LOG_INFO, "You are user #%01i", am_user);
    TraceLog(LOG_INFO, "There are currently %01i users", users);

	// GET CURRENT BOARD DATA
	if (!am_admin){
		send_message((string(1, MSG_USER_JOIN) + int_to_hex(am_user)).data());
		bool got_join_data = 0;
		BeginDrawing();
		ClearBackground(RAYWHITE);
		DrawText("Getting current board data...", GetScreenWidth() / 2 - MeasureText("Getting current board data...", 10) / 2, GetScreenHeight() / 2 - 10, 10, GRAY);
		DrawText("This might take some time", GetScreenWidth() / 2 - MeasureText("This might take some time", 10) / 2, GetScreenHeight() / 2 + 10, 10, GRAY);
		EndDrawing();
		while (!got_join_data){
			while (has_message()){
				stringstream msg(get_message());
				TraceLog(LOG_INFO, "Received: %s", msg.str().data());
				if (msg.str()[0] == MSG_MESSAGE_TO_NEW){
					unsigned char type = msg.str()[1];
					int user = hex_to_int(msg.str().substr(2, 10));
					int to_user = hex_to_int(msg.str().substr(msg.str().size() - 10, 10));
					if (to_user != am_user){
						continue;
					}
					if (type == TYPE_DRAW){
						int ptr = 12;
						cout << 1;
						int get_size = hex_to_int(msg.str().substr(ptr, 10));
						cout << '\n' << get_size << '\n';
						ptr += 10;
						vector <Vector2> get_draw;
						for (int _ = 0; _ < get_size; _++){
							cout << 'e';
							float get_x = hex_to_float(msg.str().substr(ptr, 8));
							ptr += 8;
							float get_y = hex_to_float(msg.str().substr(ptr, 8));
							ptr += 8;
							cout << 'f';
							get_draw.push_back({get_x, get_y});
						}
						float get_thick = hex_to_float(msg.str().substr(ptr, 8));
						cout << 'g';
						ptr += 8;
						Color get_color;
						get_color.r = msg.str()[ptr++];
						get_color.g = msg.str()[ptr++];
						get_color.b = msg.str()[ptr++];
						get_color.a = msg.str()[ptr++];
						cout << 'h';
						draws.push_back(get_draw);
						cout << 'i';
						draw_thicks.push_back(get_thick);
						cout << 'j';
						draw_colors.push_back(get_color);
						cout << 'k';
						action.push_back(0);
						by_user.push_back(user);
						deleted.push_back(0);
						message_history.push_back(msg.str());
					}
					else if (type == TYPE_TEXT){
						int ptr = 12;
						cout << 2;
						int get_size = hex_to_int(msg.str().substr(ptr, 10));
						cout << '\n' << get_size << '\n';
						ptr += 10;
						string get_text = msg.str().substr(ptr, get_size);
						ptr += get_size;
						float get_x = hex_to_float(msg.str().substr(ptr, 8));
						ptr += 8;
						float get_y = hex_to_float(msg.str().substr(ptr, 8));
						ptr += 8;
						cout << 'f';
						Vector2 get_pos = {get_x, get_y};
						float get_thick = hex_to_float(msg.str().substr(ptr, 8));
						cout << 'g';
						ptr += 8;
						Color get_color;
						get_color.r = msg.str()[ptr++];
						get_color.g = msg.str()[ptr++];
						get_color.b = msg.str()[ptr++];
						get_color.a = msg.str()[ptr++];
						texts.push_back(get_pos);
						cout << 'h';
						//get_text += '\0';
						text_texts.push_back(get_text);
						cout << 'i';
						text_sizes.push_back(get_thick);
						cout << 'j';
						text_colors.push_back(get_color);
						cout << 'k';

						action.push_back(1);
						by_user.push_back(user);
						deleted.push_back(0);
						message_history.push_back(msg.str());
					}
					else if (type == TYPE_UNDO){
						int i = by_user.size() - 1;
						for (;deleted[i] || by_user[i] != (int)user; i--){}
						deleted[i] = 1;/*
						cout << "deleted draw on position " << i << "\ndeleted now looks like this:\n";
						for (int j = 0; j < deleted.size(); j++){
							cout << int(deleted[j]) << ' ';
						}
						cout << "\nby_user looks like this:\n";
						for (int j = 0; j < deleted.size(); j++){
							cout << by_user[j] << ' ';
						}
						cout << '\n';*/
						message_history.push_back(msg.str());
					}
				}
				else if (msg.str()[0] == MSG_END_TO_NEW){
					int to_user = hex_to_int(msg.str().substr(1, 10));
					if (to_user == am_user){
						got_join_data = 1;
					}
				}
			}
			if (WindowShouldClose()){
    			close_network();
				CloseWindow();
				return 0;
			}
		}
	}
    
	while (!WindowShouldClose()){

		// RECIEVE MESSAGES
        while (has_message()){
            stringstream msg(get_message());
            TraceLog(LOG_INFO, "Received: %s", msg.str().data());
            if (msg.str()[0] == MSG_MESSAGE_TO_EVERYONE){
                unsigned char type = msg.str()[1];
                int user = hex_to_int(msg.str().substr(2, 10));
                if (type == TYPE_DRAW){
                    int ptr = 12;
                    int get_size = hex_to_int(msg.str().substr(ptr, 10));
                    ptr += 10;
                    vector <Vector2> get_draw;
                    for (int _ = 0; _ < get_size; _++){
                        float get_x = hex_to_float(msg.str().substr(ptr, 8));
                        ptr += 8;
                        float get_y = hex_to_float(msg.str().substr(ptr, 8));
                        ptr += 8;
                        get_draw.push_back({get_x, get_y});
                    }
                    float get_thick = hex_to_float(msg.str().substr(ptr, 8));
                    ptr += 8;
                    Color get_color;
                    get_color.r = msg.str()[ptr++];
                    get_color.g = msg.str()[ptr++];
                    get_color.b = msg.str()[ptr++];
                    get_color.a = msg.str()[ptr++];
                    draws.push_back(get_draw);
                    draw_thicks.push_back(get_thick);
                    draw_colors.push_back(get_color);
                    action.push_back(0);
                	by_user.push_back(user);
                    deleted.push_back(0);
					message_history.push_back(msg.str());
                }
                else if (type == TYPE_TEXT){
                    int ptr = 12;
                    int get_size = hex_to_int(msg.str().substr(ptr, 10));
                    ptr += 10;
					string get_text = msg.str().substr(ptr, get_size);
					ptr += get_size;
                    float get_x = hex_to_float(msg.str().substr(ptr, 8));
                    ptr += 8;
                    float get_y = hex_to_float(msg.str().substr(ptr, 8));
                    ptr += 8;
					Vector2 get_pos = {get_x, get_y};
                    float get_thick = hex_to_float(msg.str().substr(ptr, 8));
                    ptr += 8;
                    Color get_color;
                    get_color.r = msg.str()[ptr++];
                    get_color.g = msg.str()[ptr++];
                    get_color.b = msg.str()[ptr++];
                    get_color.a = msg.str()[ptr++];
					texts.push_back(get_pos);
					//get_text += '\0';
                    text_texts.push_back(get_text);
                    text_sizes.push_back(get_thick);
                    text_colors.push_back(get_color);

                    action.push_back(1);
                	by_user.push_back(user);
                    deleted.push_back(0);
					message_history.push_back(msg.str());
                }
                else if (type == TYPE_UNDO){
                    int i = by_user.size() - 1;
                    for (;deleted[i] || by_user[i] != (int)user; i--){}
                    deleted[i] = 1;/*
                    cout << "deleted draw on position " << i << "\ndeleted now looks like this:\n";
					for (int j = 0; j < deleted.size(); j++){
						cout << int(deleted[j]) << ' ';
					}
                    cout << "\nby_user looks like this:\n";
					for (int j = 0; j < deleted.size(); j++){
						cout << by_user[j] << ' ';
					}
					cout << '\n';*/
					message_history.push_back(msg.str());
                }
                else if (type == TYPE_CURS){
                    float get_x = hex_to_float(msg.str().substr(12, 8));
                    float get_y = hex_to_float(msg.str().substr(20, 8));
					Color get_color;
					get_color.r = msg.str()[28];
					get_color.g = msg.str()[29];
					get_color.b = msg.str()[30];
					get_color.a = msg.str()[31];
					cursors[user].pos = {get_x, get_y};
					cursors[user].col = get_color;
                }
            }
			else if (msg.str()[0] == MSG_USER_JOIN){
				new_users.push_back(hex_to_int(msg.str().substr(1, 10)));
				if (am_admin){
					sending_to_new = 1;
				}
				users++;
    			TraceLog(LOG_INFO, "User #%01i joined", hex_to_int(msg.str().substr(1, 10)));
    			TraceLog(LOG_INFO, "There are currently %01i users", users);
			}
			else if (msg.str()[0] == MSG_USER_LEAVE){
				int user = hex_to_int(msg.str().substr(1, 10));
				for (int i = 0; i < action.size(); i++){
					if (by_user[i] == user){
						deleted[i] = true;
					}
				}
				users--;
    			TraceLog(LOG_INFO, "User #%01i left", user);
    			TraceLog(LOG_INFO, "There are currently %01i users", users);
			}
        }
		// SEND BOARD DATA
		if (sending_to_new){
			TraceLog(LOG_INFO, "Sending data to user #%01i: %01i", new_users[0], send_to_new_idx);
			for (int i = 0; i < SEND_PER_FRAME && send_to_new_idx < message_history.size(); i++){
				string msg = message_history[send_to_new_idx];
				msg[0] = MSG_MESSAGE_TO_NEW;
				msg += int_to_hex(new_users[0]);
				send_message(msg);
				send_to_new_idx++;
			}
			if (send_to_new_idx == message_history.size()){
				cout << 'b';
				send_to_new_idx = 0;
				string msg;
				msg += MSG_END_TO_NEW;
				msg += int_to_hex(new_users[0]);
				send_message(msg);
				new_users.erase(new_users.begin());
				if (new_users.size() == 0){
					sending_to_new = false;
				}
			}
		}

		cam.offset = Vector2{GetScreenWidth() / 2, GetScreenHeight() / 2};
		// SIDE BAR
		side_bar = Rectangle{side_bar.x, GetScreenHeight() / 2 - 300, 175, 600};
		side_bar_open = Rectangle{side_bar.x + 150, GetScreenHeight() / 2 - 50, 50, 100};
		color_pick = Rectangle{side_bar.x + 120 + color_anim, GetScreenHeight() / 2 - 90, 370, 180};
		button_selected = -1;
		for (int i = 0; i < buttons; i++){
			if (CheckCollisionPointCircle(GetMousePosition(), Vector2{side_bar.x + 130, side_bar.y + side_bar.height / 2 + (i - (buttons - 1) / 2.0) * ((BUTTON_BASE_SIZE + BUTTON_SELECTED_SIZE) / 2 + 5) * 2}, button_size[i])){
				button_selected = i;
				break;
			}
		}
		for (int i = 0; i < buttons; i++){
			button_target_size[i] = BUTTON_BASE_SIZE;
			if (i == button_selected || mode == i + 1){
				button_target_size[i] = BUTTON_SELECTED_SIZE;
			}
			button_size[i] = (button_size[i] * 4 + button_target_size[i]) / 5;
		}
		if (button_selected != -1 && IsMouseButtonPressed(0)){
			mode = button_selected + 1;
			if (text){
				action.push_back(1);
                by_user.push_back(am_user);
				texts.push_back(text_pos);
				text_texts.push_back(text_text);
				text_colors.push_back(draw_color);
				text_sizes.push_back(draw_thick * 2);
                deleted.push_back(0);

				stringstream msg;
				msg << MSG_MESSAGE_TO_EVERYONE;
				msg << TYPE_TEXT;
				msg << int_to_hex(am_user);
				msg << int_to_hex(text_text.size());
				msg << text_text;
				msg << float_to_hex(text_pos.x) << float_to_hex(text_pos.y);
				msg << float_to_hex(draw_thick * 2);
				msg << draw_color.r;
				msg << draw_color.g;
				msg << draw_color.b;
				msg << draw_color.a;
				send_message(msg.str());
				message_history.push_back(msg.str());
			}
			text = false;
			PlaySound(ButtonSnd);
		}
		if (mode == 8){
			color_anim = color_anim * 4 / 5;
			if (IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {color_pick.x + 100, color_pick.y + 10, 255, 40})){
				color_selected = 1;
			}
			if (IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {color_pick.x + 100, color_pick.y + 70, 255, 40})){
				color_selected = 2;
			}
			if (IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {color_pick.x + 100, color_pick.y + 130, 255, 40})){
				color_selected = 3;
			}
			if (IsMouseButtonDown(0) && color_selected == 1){
				draw_color.r = max(min(GetMouseX() - color_pick.x - 98, 255), 0);
			}
			if (IsMouseButtonDown(0) && color_selected == 2){
				draw_color.g = max(min(GetMouseX() - color_pick.x - 98, 255), 0);
			}
			if (IsMouseButtonDown(0) && color_selected == 3){
				draw_color.b = max(min(GetMouseX() - color_pick.x - 98, 255), 0);
			}
			if (IsMouseButtonReleased(0)){
				color_selected = -1;
			}
		}
		else{
			color_anim = (color_anim * 4 - 400) / 5;
		}
		if (CheckCollisionPointRec(GetMousePosition(), side_bar_open) && IsMouseButtonPressed(0)){
			if (side_bar_opened){
				PlaySound(CloseSnd);
			}
			else{
				PlaySound(OpenSnd);
			}
			side_bar_opened = 1 - side_bar_opened;
		}
		if (side_bar_opened){
			side_bar.x = (side_bar.x * 3 - 100) / 4;
		}
		else{
			side_bar.x = (side_bar.x * 3 - 179) / 4;
		}

		// KEYBOARD SHORTCUTS
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_R)){
			draw_color = {GetRandomValue(0, 255), GetRandomValue(0, 255), GetRandomValue(0, 255), 255};
		}
		if (mode != 7){
			draw_thick += GetMouseWheelMove() / cam.zoom;
			draw_thick = max(draw_thick, 0);
		}
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z) && by_user.size() > 0){
            int i = by_user.size() - 1;
			for (;i >= 0 && (deleted[i] || by_user[i] != am_user); i--){}
            if (i != -1){
                deleted[i] = 1;
                stringstream msg;
                msg << MSG_MESSAGE_TO_EVERYONE;
                msg << TYPE_UNDO;
                msg << int_to_hex(am_user);
                send_message(msg.str());
				message_history.push_back(msg.str());/*
                cout << "deleted draw on position " << i << "\ndeleted now looks like this:\n";
				for (int j = 0; j < deleted.size(); j++){
					cout << int(deleted[j]) << ' ';
				}
                cout << "\nby_user looks like this:\n";
				for (int j = 0; j < deleted.size(); j++){
					cout << int(by_user[j]) << ' ';
				}
				cout << '\n';*/
            }
		}
		if (IsKeyDown(KEY_LEFT_CONTROL)){
			cam.zoom *= 1 + GetMouseWheelMove() / 10;
		}
		
		// MODE CONTROL
		SetMouseCursor(MOUSE_CURSOR_DEFAULT);
		if (mode == 1){
			//DRAWING
			if (IsMouseButtonPressed(0) && button_selected == -1 && !CheckCollisionPointRec(GetMousePosition(), side_bar) && !CheckCollisionPointRec(GetMousePosition(), side_bar_open)){
				drawing = true;
				draw_start = frame;
				draw_traj.push_back(GetMousePositionCam(cam));
			}
			else if (IsMouseButtonDown(0) && drawing && (frame - draw_start) % INTERVAL == 0){
				draw_traj.push_back(GetMousePositionCam(cam));
			}
			if (IsMouseButtonReleased(0) && drawing){
				drawing = false;
				action.push_back(0);
                by_user.push_back(am_user);
				draw_traj.push_back(GetMousePositionCam(cam));
				draws.push_back(draw_traj);
				draw_colors.push_back(draw_color);
				draw_thicks.push_back(draw_thick);
                deleted.push_back(0);

                // Send drawing data to server
                stringstream msg;
                msg << MSG_MESSAGE_TO_EVERYONE;
                msg << TYPE_DRAW;
                msg << int_to_hex(am_user);
                msg << int_to_hex(draw_traj.size());
                for (int _ = 0; _ < draw_traj.size(); _++){
                    msg << float_to_hex(draw_traj[_].x) << float_to_hex(draw_traj[_].y);
                }
                msg << float_to_hex(draw_thick);
                msg << draw_color.r << draw_color.g << draw_color.b << draw_color.a;
                send_message(msg.str());
				message_history.push_back(msg.str());

				draw_traj.clear();
			}
		}
		else if (mode == 2){
			// COPYING
			if (IsMouseButtonDown(0) && button_selected == -1 && !CheckCollisionPointRec(GetMousePosition(), side_bar) && !CheckCollisionPointRec(GetMousePosition(), side_bar_open)){
				bool copied = false;
				for (int i = draws.size() - 1; i >= 0 && !copied; i--){
					for (int j = 0; j < draws[i].size() && !copied; j++){
						if (CheckCollisionPointCircle(GetMousePositionCam(cam), draws[i][j], draw_thicks[i] / 2)){
							draw_color = draw_colors[i];
							copied = true;
						}
						else if (j < draws[i].size() - 1 && CheckCollisionPointLine(GetMousePositionCam(cam), draws[i][j], draws[i][j + 1], draw_thicks[i] / 2)){
							draw_color = draw_colors[i];
							copied = true;
						}
					}
				}
				if (!copied){
					draw_color = BG_COL;
				}
			}
		}
		else if (mode == 3){
			// LINE
			if (IsMouseButtonPressed(0) && button_selected == -1 && !CheckCollisionPointRec(GetMousePosition(), side_bar) && !CheckCollisionPointRec(GetMousePosition(), side_bar_open)){
				line = true;
				line_start = GetMousePositionCam(cam);
			}
			if (IsMouseButtonDown(0) && line){
				line_end = GetMousePositionCam(cam);
			}
			if (IsMouseButtonReleased(0) && line){
				line_end = GetMousePositionCam(cam);
				vector <Vector2> cur_line(2);
				cur_line[0] = line_start;
				cur_line[1] = line_end;
				action.push_back(0);
                by_user.push_back(am_user);
				draws.push_back(cur_line);
				draw_colors.push_back(draw_color);
				draw_thicks.push_back(draw_thick);
                deleted.push_back(0);
				line = false;

                // Send drawing data to server
                stringstream msg;
                msg << MSG_MESSAGE_TO_EVERYONE;
                msg << TYPE_DRAW;
                msg << int_to_hex(am_user);
                msg << int_to_hex(cur_line.size());
                for (int _ = 0; _ < cur_line.size(); _++){
                    msg << float_to_hex(cur_line[_].x) << float_to_hex(cur_line[_].y);
                }
                msg << float_to_hex(draw_thick);
                msg << draw_color.r << draw_color.g << draw_color.b << draw_color.a;
                send_message(msg.str());
				message_history.push_back(msg.str());
			}
		}
		else if (mode == 4){
			// RECTANGLE
			if (IsMouseButtonPressed(0) && button_selected == -1 && !CheckCollisionPointRec(GetMousePosition(), side_bar) && !CheckCollisionPointRec(GetMousePosition(), side_bar_open)){
				rec = true;
				rec_start = GetMousePositionCam(cam);
			}
			if (IsMouseButtonDown(0) && rec){
				rec_end = GetMousePositionCam(cam);
			}
			if (IsMouseButtonReleased(0) && rec){
				rec_end = GetMousePositionCam(cam);
				vector <Vector2> cur_rec(5);
				cur_rec[0] = rec_start;
				cur_rec[1] = Vector2{rec_end.x, rec_start.y};
				cur_rec[2] = rec_end;
				cur_rec[3] = Vector2{rec_start.x, rec_end.y};
				cur_rec[4] = rec_start;
				action.push_back(0);
                by_user.push_back(am_user);
				draws.push_back(cur_rec);
				draw_colors.push_back(draw_color);
				draw_thicks.push_back(draw_thick);
                deleted.push_back(0);
				rec = false;

                // Send drawing data to server
                stringstream msg;
                msg << MSG_MESSAGE_TO_EVERYONE;
                msg << TYPE_DRAW;
                msg << int_to_hex(am_user);
                msg << int_to_hex(cur_rec.size());
                for (int _ = 0; _ < cur_rec.size(); _++){
                    msg << float_to_hex(cur_rec[_].x) << float_to_hex(cur_rec[_].y);
                }
                msg << float_to_hex(draw_thick);
                msg << draw_color.r << draw_color.g << draw_color.b << draw_color.a;
                send_message(msg.str());
				message_history.push_back(msg.str());
			}
		}
		else if (mode == 5){
			// ELLIPSE
			if (IsMouseButtonPressed(0) && button_selected == -1 && !CheckCollisionPointRec(GetMousePosition(), side_bar) && !CheckCollisionPointRec(GetMousePosition(), side_bar_open)){
				elip = true;
				elip_start = GetMousePositionCam(cam);
			}
			if (IsMouseButtonDown(0) && elip){
				elip_end = GetMousePositionCam(cam);
			}
			if (IsMouseButtonReleased(0) && elip){
				elip_end = GetMousePositionCam(cam);
				Vector2 elip_center = Vector2{(elip_start.x + elip_end.x) / 2, (elip_start.y + elip_end.y) / 2};
				Vector2 elip_scale = Vector2{(elip_end.x - elip_start.x) / 2, (elip_end.y - elip_start.y) / 2};
				vector <Vector2> cur_elip(ELLIPSE_ANGLES + 1);
				for (int i = 0; i < ELLIPSE_ANGLES + 1; i++){
					cur_elip[i] = Vector2{cos(i * 2 * PI / ELLIPSE_ANGLES) * elip_scale.x + elip_center.x, sin(i * 2 * PI / ELLIPSE_ANGLES) * elip_scale.y + elip_center.y};
				}
				action.push_back(0);
                by_user.push_back(am_user);
				draws.push_back(cur_elip);
				draw_colors.push_back(draw_color);
				draw_thicks.push_back(draw_thick);
                deleted.push_back(0);
				elip = false;

                // Send drawing data to server
                stringstream msg;
                msg << MSG_MESSAGE_TO_EVERYONE;
                msg << TYPE_DRAW;
                msg << int_to_hex(am_user);
                msg << int_to_hex(cur_elip.size());
                for (int _ = 0; _ < cur_elip.size(); _++){
                    msg << float_to_hex(cur_elip[_].x) << float_to_hex(cur_elip[_].y);
                }
                msg << float_to_hex(draw_thick);
                msg << draw_color.r << draw_color.g << draw_color.b << draw_color.a;
                send_message(msg.str());
				message_history.push_back(msg.str());
			}
		}
		else if (mode == 6){
			// TEXT
			SetMouseCursor(MOUSE_CURSOR_IBEAM);
			if (IsMouseButtonPressed(0) && !CheckCollisionPointRec(GetMousePosition(), side_bar) && !CheckCollisionPointRec(GetMousePosition(), side_bar_open)){
				if (text){
					action.push_back(1);
                    by_user.push_back(am_user);
					texts.push_back(text_pos);
					text_texts.push_back(text_text);
					text_colors.push_back(draw_color);
					text_sizes.push_back(draw_thick * 2);
                    deleted.push_back(0);

					stringstream msg;
					msg << MSG_MESSAGE_TO_EVERYONE;
					msg << TYPE_TEXT;
					msg << int_to_hex(am_user);
					msg << int_to_hex(text_text.size());
					msg << text_text;
					msg << float_to_hex(text_pos.x) << float_to_hex(text_pos.y);
					msg << float_to_hex(draw_thick * 2);
					msg << draw_color.r;
					msg << draw_color.g;
					msg << draw_color.b;
					msg << draw_color.a;
					send_message(msg.str());
					message_history.push_back(msg.str());
				}
				text = true;
				text_text = "";
				text_pos = {GetMousePositionCam(cam).x, GetMousePositionCam(cam).y - 3 - draw_thick};
			}
			char chr = GetCharPressed();
			if (chr != 0){
				text_text += chr;
			}
			if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && text_text.size() > 0){
				text_text.pop_back();
			}
			if (IsKeyPressed(KEY_ENTER)){
				action.push_back(1);
                by_user.push_back(am_user);
				texts.push_back(text_pos);
				text_texts.push_back(text_text);
				text_colors.push_back(draw_color);
				text_sizes.push_back(draw_thick * 2);
                deleted.push_back(0);
				text = false;

				stringstream msg;
				msg << MSG_MESSAGE_TO_EVERYONE;
				msg << TYPE_TEXT;
				msg << int_to_hex(am_user);
				msg << int_to_hex(text_text.size());
				msg << text_text;
				msg << float_to_hex(text_pos.x) << float_to_hex(text_pos.y);
				msg << float_to_hex(draw_thick * 2);
				msg << draw_color.r;
				msg << draw_color.g;
				msg << draw_color.b;
				msg << draw_color.a;
				send_message(msg.str());
				message_history.push_back(msg.str());
			}
		}
		else if (mode == 7){
			// HAND
			SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
			if (IsMouseButtonPressed(0)){
				grab_start = GetMousePosition();
			}
			if (IsMouseButtonDown(0)){
				cam.target = Vector2{(grab_start.x - GetMousePosition().x) / cam.zoom + prev_target.x, (grab_start.y - GetMousePosition().y) / cam.zoom + prev_target.y};
			}
			if (IsMouseButtonReleased(0)){
				prev_target = cam.target;
			}
			cam.zoom *= 1 + GetMouseWheelMove() / 10;
		}
		else if (mode == 8){
			if (IsMouseButtonPressed(0) && !CheckCollisionPointRec(GetMousePosition(), side_bar) && !CheckCollisionPointRec(GetMousePosition(), color_pick)){
				mode = 1;
				drawing = true;
				draw_start = frame;
				draw_traj.push_back(GetMousePositionCam(cam));
				PlaySound(ButtonSnd);
			}
		}

		// SEND CURSOR POSITION
		if (frame % CURSOR_INTERVAL == 0){
			stringstream msg;
			msg << MSG_MESSAGE_TO_EVERYONE;
			msg << TYPE_CURS;
			msg << int_to_hex(am_user);
			msg << float_to_hex(GetMousePositionCam(cam).x) << float_to_hex(GetMousePositionCam(cam).y);
			msg << draw_color.r;
			msg << draw_color.g;
			msg << draw_color.b;
			msg << draw_color.a;

			send_message(msg.str());
		}
		
		// SCREEN OUTPUT
		BeginDrawing();
			BeginMode2D(cam);
				ClearBackground(BG_COL);
				// DRAW CROSSES
				if (cam.zoom > 0.1){
					for (int i = int(cam.target.x - cam.offset.x / cam.zoom) / CROSS_INTERVAL * CROSS_INTERVAL - CROSS_INTERVAL; i <= int(cam.target.x + cam.offset.x / cam.zoom) / CROSS_INTERVAL * CROSS_INTERVAL + CROSS_INTERVAL; i += CROSS_INTERVAL){
						for (int j = int(cam.target.y - cam.offset.y / cam.zoom) / CROSS_INTERVAL * CROSS_INTERVAL - CROSS_INTERVAL; j <= int(cam.target.y + cam.offset.y / cam.zoom) / CROSS_INTERVAL * CROSS_INTERVAL + CROSS_INTERVAL; j += CROSS_INTERVAL){
							DrawLineEx({i, j - 16}, {i, j + 16}, 5, CROSS_COL);
							DrawLineEx({i - 16, j}, {i + 16, j}, 5, CROSS_COL);
						}
					}
				}

				// DRAW BOARD DATA
				for (int i = 0, draw_idx = 0, text_idx = 0; i < action.size(); i++){
					if (action[i] == 0){
                        if (deleted[i]){draw_idx++; continue;}
						for (int j = 0; j < draws[draw_idx].size(); j++){
							if (j != draws[draw_idx].size() - 1){
								DrawLineEx(draws[draw_idx][j], draws[draw_idx][j + 1], draw_thicks[draw_idx], draw_colors[draw_idx]);
							}
							DrawCircleV(draws[draw_idx][j], draw_thicks[draw_idx] / 2, draw_colors[draw_idx]);
						}
						draw_idx++;
					}
					else if (action[i] == 1){
                        if (deleted[i]){text_idx++; continue;}
						DrawTextEx(font, text_texts[text_idx].data(), texts[text_idx], text_sizes[text_idx], 0, text_colors[text_idx]);
						text_idx++;
					}
				}

				// DRAW CURRENT LINE OR TEXT
				for (int i = 0; i < draw_traj.size(); i++){
					if (i != draw_traj.size() - 1){
						DrawLineEx(draw_traj[i], draw_traj[i + 1], draw_thick, draw_color);
					}
					DrawCircleV(draw_traj[i], draw_thick / 2, draw_color);
				}
				if (line){
					DrawCircleV(line_start, draw_thick / 2, draw_color);
					DrawLineEx(line_start, line_end, draw_thick, draw_color);
					DrawCircleV(line_end, draw_thick / 2, draw_color);
				}
				else if (rec){
					DrawCircleV(rec_start, draw_thick / 2, draw_color);
					DrawCircleV(Vector2{rec_end.x, rec_start.y}, draw_thick / 2, draw_color);
					DrawCircleV(rec_end, draw_thick / 2, draw_color);
					DrawCircleV(Vector2{rec_start.x, rec_end.y}, draw_thick / 2, draw_color);
					DrawLineEx(rec_start, Vector2{rec_end.x, rec_start.y}, draw_thick, draw_color);
					DrawLineEx(Vector2{rec_end.x, rec_start.y}, rec_end, draw_thick, draw_color);
					DrawLineEx(rec_end, Vector2{rec_start.x, rec_end.y}, draw_thick, draw_color);
					DrawLineEx(Vector2{rec_start.x, rec_end.y}, rec_start, draw_thick, draw_color);
				}
				else if (elip){
					Vector2 elip_center = Vector2{(elip_start.x + elip_end.x) / 2, (elip_start.y + elip_end.y) / 2};
					Vector2 elip_scale = Vector2{(elip_end.x - elip_start.x) / 2, (elip_end.y - elip_start.y) / 2};
					for (int i = 0; i < ELLIPSE_ANGLES; i++){
						DrawCircleV(Vector2{cos(i * 2 * PI / ELLIPSE_ANGLES) * elip_scale.x + elip_center.x, sin(i * 2 * PI / ELLIPSE_ANGLES) * elip_scale.y + elip_center.y}, draw_thick / 2, draw_color);
						DrawLineEx(Vector2{cos(i * 2 * PI / ELLIPSE_ANGLES) * elip_scale.x + elip_center.x, sin(i * 2 * PI / ELLIPSE_ANGLES) * elip_scale.y + elip_center.y},
								   Vector2{cos((i + 1) * 2 * PI / ELLIPSE_ANGLES) * elip_scale.x + elip_center.x, sin((i + 1) * 2 * PI / ELLIPSE_ANGLES) * elip_scale.y + elip_center.y}, draw_thick, draw_color);
					}
				}
				else if (text){
					DrawRectangleLines(text_pos.x - 3, text_pos.y - 3, MeasureTextEx(font, text_text.data(), draw_thick * 2, 0).x + 5 + draw_thick, 6 + draw_thick * 2, BLACK);
					string draw_text = text_text.data();
					if (frame % 60 < 30){
						draw_text += '|';
					}
					DrawTextEx(font, draw_text.data(), text_pos, draw_thick * 2, 0, draw_color);
				}

				// DRAW CURSORS
				for (auto i : cursors){
					if (i.second.col.r > 0 ||  i.second.col.g > 0 || i.second.col.b > 0){
						DrawTextureEx(Cursorspr, i.second.pos, 0, 1 / cam.zoom, i.second.col);
					}
					else{
						DrawTextureEx(Cursorspr, i.second.pos, 0, 1 / cam.zoom, WHITE);
					}
				}
			EndMode2D();

			if (mode == 1 || mode == 3 || mode == 4 || mode == 5 || mode == 8){
				DrawCircleLinesV(GetMousePosition(), draw_thick * cam.zoom / 2, draw_color);
			}
			else if (mode == 6){
				DrawRectangleLines(GetMouseX() - 3 * cam.zoom, GetMouseY() - (6 + draw_thick) * cam.zoom, (5 + draw_thick) * cam.zoom, (6 + draw_thick * 2) * cam.zoom, draw_color);
			}

			// DRAW SIDEBAR
			DrawRectangleRounded(color_pick, 0.1, 15, BAR_COL);
			DrawRectangleRoundedLinesEx(color_pick, 0.1, 15, 4, BAR_LINE_COL);

			DrawRectangleGradientH(color_pick.x + 100, color_pick.y + 10, 255, 40, {0, draw_color.g, draw_color.b, 255}, {255, draw_color.g, draw_color.b, 255});
			DrawRectangleGradientH(color_pick.x + 100, color_pick.y + 70, 255, 40, {draw_color.r, 0, draw_color.b, 255}, {draw_color.r, 255, draw_color.b, 255});
			DrawRectangleGradientH(color_pick.x + 100, color_pick.y + 130, 255, 40, {draw_color.r, draw_color.g, 0, 255}, {draw_color.r, draw_color.g, 255, 255});
			DrawRectangleRounded({color_pick.x + 90 + draw_color.r, color_pick.y + 10, 20, 40}, 0.4, 15, draw_color);
			DrawRectangleRoundedLinesEx({color_pick.x + 90 + draw_color.r, color_pick.y + 10, 20, 40}, 0.4, 15, 4, BAR_LINE_COL);
			DrawRectangleRounded({color_pick.x + 90 + draw_color.g, color_pick.y + 70, 20, 40}, 0.4, 15, draw_color);
			DrawRectangleRoundedLinesEx({color_pick.x + 90 + draw_color.g, color_pick.y + 70, 20, 40}, 0.4, 15, 4, BAR_LINE_COL);
			DrawRectangleRounded({color_pick.x + 90 + draw_color.b, color_pick.y + 130, 20, 40}, 0.4, 15, draw_color);
			DrawRectangleRoundedLinesEx({color_pick.x + 90 + draw_color.b, color_pick.y + 130, 20, 40}, 0.4, 15, 4, BAR_LINE_COL);

			DrawRectangleRounded(side_bar_open, 0.4, 15, BAR_COL);
			DrawRectangleRoundedLinesEx(side_bar_open, 0.4, 15, 4, BAR_LINE_COL);
			DrawRectangleRounded(side_bar, 0.4, 15, BAR_COL);
			DrawRectangleRoundedLinesEx(side_bar, 0.4, 15, 4, BAR_LINE_COL);
			if (side_bar_opened){
				DrawTexture(Close, side_bar_open.x + 32, side_bar_open.y + side_bar_open.height / 2 - 13, WHITE);
			}
			else{
				DrawTexture(Open, side_bar_open.x + 33, side_bar_open.y + side_bar_open.height / 2 - 13, WHITE);
			}
			for (int i = 0; i < buttons; i++){
				Color temp = BUT_COL;
				if (mode == i + 1){
					temp = BUT_SELECTED_COL;
					if (mode == 2){
						temp = draw_color;
					}
				}
				DrawCircleV(Vector2{side_bar.x + 130, side_bar.y + side_bar.height / 2 + (i - (buttons - 1) / 2.0) * ((BUTTON_BASE_SIZE + BUTTON_SELECTED_SIZE) / 2 + 5) * 2}, button_size[i] + 3, BUT_LINE_COL);
				DrawCircleV(Vector2{side_bar.x + 130, side_bar.y + side_bar.height / 2 + (i - (buttons - 1) / 2.0) * ((BUTTON_BASE_SIZE + BUTTON_SELECTED_SIZE) / 2 + 5) * 2}, button_size[i], temp);
				DrawTexturePro(button_texture[i], Rectangle{0, 0, 500, 500}, Rectangle{side_bar.x + 130, side_bar.y + side_bar.height / 2 + (i - (buttons - 1) / 2.0) * ((BUTTON_BASE_SIZE + BUTTON_SELECTED_SIZE) / 2 + 5) * 2, 32 * button_size[i] / BUTTON_BASE_SIZE, 32 * button_size[i] / BUTTON_BASE_SIZE}, {(32 * button_size[i] / BUTTON_BASE_SIZE) / 2, (32 * button_size[i] / BUTTON_BASE_SIZE) / 2}, 0, WHITE);
			}
		EndDrawing();

		frame++;
	}
    close_network();
	CloseWindow();
}
