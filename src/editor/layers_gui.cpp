//  SuperTux
//  Copyright (C) 2015 Hume2 <teratux.mail@gmail.com>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "editor/layers_gui.hpp"

#include "editor/editor.hpp"
#include "editor/layer_icon.hpp"
#include "editor/object_menu.hpp"
#include "editor/tip.hpp"
#include "gui/menu_manager.hpp"
#include "math/vector.hpp"
#include "object/camera.hpp"
#include "object/tilemap.hpp"
#include "supertux/colorscheme.hpp"
#include "supertux/menu/menu_storage.hpp"
#include "supertux/resources.hpp"
#include "supertux/sector.hpp"
#include "video/drawing_context.hpp"
#include "video/renderer.hpp"
#include "video/video_system.hpp"
#include "video/viewport.hpp"

EditorLayersGui::EditorLayersGui(Editor& editor) :
  m_editor(editor),
  m_layer_icons(),
  m_selected_tilemap(),
  m_Ypos(448),
  m_Width(512),
  m_sector_text(),
  m_sector_text_width(0),
  m_hovered_item(HI_NONE),
  m_hovered_layer(-1),
  m_object_tip()
{
}

void
EditorLayersGui::draw(DrawingContext& context)
{

  if (m_object_tip) {
    auto position = get_layer_coords(m_hovered_layer);
    m_object_tip->draw_up(context, position);
  }

  context.color().draw_filled_rect(Rectf(Vector(0, static_cast<float>(m_Ypos)),
                                         Vector(static_cast<float>(m_Width), static_cast<float>(SCREEN_HEIGHT))),
                                     Color(0.9f, 0.9f, 1.0f, 0.6f),
                                     0.0f,
                                     LAYER_GUI-10);

  Rectf target_rect = Rectf(0, 0, 0, 0);
  bool draw_rect = true;

  switch (m_hovered_item)
  {
    case HI_SPAWNPOINTS:
      target_rect = Rectf(Vector(0, static_cast<float>(m_Ypos)),
                          Vector(static_cast<float>(m_Xpos), static_cast<float>(SCREEN_HEIGHT)));
      break;

    case HI_SECTOR:
      target_rect = Rectf(Vector(static_cast<float>(m_Xpos), static_cast<float>(m_Ypos)),
                          Vector(static_cast<float>(m_sector_text_width + m_Xpos), static_cast<float>(SCREEN_HEIGHT)));
      break;

    case HI_LAYERS:
      {
        Vector coords = get_layer_coords(m_hovered_layer);
        target_rect = Rectf(coords, coords + Vector(32, 32));
      }
      break;

    default:
      draw_rect = false;
      break;
  }

  if (draw_rect)
  {
    context.color().draw_filled_rect(target_rect, Color(0.9f, 0.9f, 1.0f, 0.6f), 0.0f,
                                       LAYER_GUI-5);
  }

  if (!m_editor.is_level_loaded()) {
    return;
  }

  context.color().draw_text(Resources::normal_font, m_sector_text,
                            Vector(35.0f, static_cast<float>(m_Ypos) + 5.0f),
                            ALIGN_LEFT, LAYER_GUI, ColorScheme::Menu::default_color);

  int pos = 0;
  for (const auto& layer_icon : m_layer_icons) {
    if (layer_icon->is_valid()) {
      layer_icon->draw(context, get_layer_coords(pos));
    }
    pos++;
  }
}

void
EditorLayersGui::update(float dt_sec)
{
  auto it = m_layer_icons.begin();
  while (it != m_layer_icons.end())
  {
    auto layer_icon = (*it).get();
    if (!layer_icon->is_valid())
      it = m_layer_icons.erase(it);
    else
      ++it;
  }
}

bool
EditorLayersGui::event(const SDL_Event& ev)
{
  switch (ev.type)
  {
    case SDL_MOUSEBUTTONDOWN:
    {
      if (ev.button.button == SDL_BUTTON_LEFT) {
        switch (m_hovered_item)
        {
          case HI_SECTOR:
            m_editor.disable_keyboard();
            MenuManager::instance().set_menu(MenuStorage::EDITOR_SECTORS_MENU);
            break;

          case HI_LAYERS:
            if (m_hovered_layer >= m_layer_icons.size()) {
              break;
            }
            if ( m_layer_icons[m_hovered_layer]->m_is_tilemap ) {
              if (m_selected_tilemap) {
                (static_cast<TileMap*>(m_selected_tilemap))->m_editor_active = false;
              }
              m_selected_tilemap = m_layer_icons[m_hovered_layer]->m_layer;
              (static_cast<TileMap*>(m_selected_tilemap))->m_editor_active = true;
              m_editor.edit_path((static_cast<TileMap*>(m_selected_tilemap))->get_path(),
                                 m_selected_tilemap);
            } else {
              auto cam = dynamic_cast<Camera*>(m_layer_icons[m_hovered_layer]->m_layer);
              if (cam) {
                m_editor.edit_path(cam->get_path(), cam);
              }
            }
            break;

          default:
            return false;
            break;
        }
      } else if (ev.button.button == SDL_BUTTON_RIGHT) {
        if (m_hovered_item == HI_LAYERS && m_hovered_layer < m_layer_icons.size()) {
          auto om = std::make_unique<ObjectMenu>(m_editor, m_layer_icons[m_hovered_layer]->m_layer);
          m_editor.deactivate_request = true;
          MenuManager::instance().push_menu(std::move(om));
        } else {
          return false;
        }
      }
    }
    break;

    case SDL_MOUSEMOTION:
    {
      Vector mouse_pos = VideoSystem::current()->get_viewport().to_logical(ev.motion.x, ev.motion.y);
      float x = mouse_pos.x - static_cast<float>(m_Xpos);
      float y = mouse_pos.y - static_cast<float>(m_Ypos);
      if (y < 0 || x > static_cast<float>(m_Width)) {
        m_hovered_item = HI_NONE;
        m_object_tip = nullptr;
        return false;
      }
      if (x < 0) {
        m_hovered_item = HI_SPAWNPOINTS;
        m_object_tip = nullptr;
        break;
      } else {
        if (x <= static_cast<float>(m_sector_text_width)) {
          m_hovered_item = HI_SECTOR;
          m_object_tip = nullptr;
        } else {
          unsigned int new_hovered_layer = get_layer_pos(mouse_pos);
          if (m_hovered_layer != new_hovered_layer || m_hovered_item != HI_LAYERS) {
            m_hovered_layer = new_hovered_layer;
            update_tip();
          }
          m_hovered_item = HI_LAYERS;
        }
      }
    }
    break;

    case SDL_WINDOWEVENT:
      if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        resize();
      }
      return false;

    default:
      return false;
  }

  return true;
}

void
EditorLayersGui::resize()
{
  m_Ypos = SCREEN_HEIGHT - 32;
  m_Width = SCREEN_WIDTH - 128;
}

void
EditorLayersGui::setup()
{
  resize();
}

void
EditorLayersGui::refresh_sector_text()
{
  m_sector_text = _("Sector") + ": " + m_editor.currentsector->get_name();
  m_sector_text_width  = int(Resources::normal_font->get_text_width(m_sector_text)) + 6;
}

void
EditorLayersGui::sort_layers()
{
  std::sort(m_layer_icons.begin(), m_layer_icons.end(),
            [](const std::unique_ptr<LayerIcon>& lhs, const std::unique_ptr<LayerIcon>& rhs) {
              return lhs->get_zpos() < rhs->get_zpos();
            });
}

void
EditorLayersGui::add_layer(GameObject* layer)
{
  auto icon = std::make_unique<LayerIcon>(layer);
  int z_pos = icon->get_zpos();

  // The icon is inserted to the correct position.
  for (auto i = m_layer_icons.begin(); i != m_layer_icons.end(); ++i) {
    const auto& li = i->get();
    if (li->get_zpos() < z_pos) {
      m_layer_icons.insert(i, move(icon));
      return;
    }
  }

  m_layer_icons.push_back(move(icon));
}

void
EditorLayersGui::update_tip()
{
  if ( m_hovered_layer >= m_layer_icons.size() ) {
    m_object_tip = nullptr;
    return;
  }
  m_object_tip = std::make_unique<Tip>(m_layer_icons[m_hovered_layer]->m_layer);
}

Vector
EditorLayersGui::get_layer_coords(const int pos) const
{
  return Vector(static_cast<float>(pos * 35 + m_Xpos + m_sector_text_width),
                static_cast<float>(m_Ypos));
}

int
EditorLayersGui::get_layer_pos(const Vector& coords) const
{
  return static_cast<int>((coords.x - static_cast<float>(m_Xpos) - static_cast<float>(m_sector_text_width)) / 35.0f);
}

/* EOF */
