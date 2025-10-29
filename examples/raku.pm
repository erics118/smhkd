grammar HotkeyConfig {
    token TOP { 
        <statement>*
    }
    
    token statement {
        | <config_property>
        | <custom_modifier>
        | <hotkey>
    }
    
    token config_property_name {
        'max_chord_interval' | 'hold_modifier_threshold' | 'simultaneous_threshold'
    }
    
    token config_property {
        <config_property_name> '=' <number>
    }
    
    token key {
        <[a..z0..9]>
    }
    
    token keycode {
        '0x' <[0..9A..F]>+
    }
    
    token literal {
        'enter' | 'space' | 'escape'
    }
    
    token identifier {
        <[a..zA..Z]> <[a..zA..Z0-9_]>*
    }
    
    token number {
        <[0..9]>+
    }
    
    token mouse {
        'left' | 'right' | 'middle'
    }
    
    token simple_keysym {
        <key> | <keycode> | <literal> | <mouse>
    }
    
    token simultaneous_keysym {
        '[' <simple_keysym>+ ',' ']'
    }
    
    token brace_expansion_keysym {
        '{' <simple_keysym>+ ',' '}'
    }
    
    token keysym {
        <simple_keysym> | <brace_expansion_keysym> | <simultaneous_keysym>
    }
    
    token base_mod {
        'cmd' | 'lcmd' | 'rcmd' | 'alt' | 'lalt' | 'ralt' | 'shift' | 'lshift' | 'rshift' | 'ctrl' | 'lctrl' | 'rctrl' | 'fn'
    }
    
    token custom_mod {
        'define_modifier' <identifier> '=' <modifiers>
    }
    
    token held_mod {
        '(' <simple_keysym> ')'
    }
    
    token modifier {
        <base_mod> | <custom_mod> | <held_mod> | <modifier> '+' <modifier>
    }
    
    token passthrough { '@' }
    token repeat { '&' }
    token on_release { '^' }
    
    token flags {
        <passthrough>? <repeat>? <on_release>? 
    }
    
    token command {
        <-[\n]>+
    }
    
    token chord {
        <keysym> | <modifier> '+' <keysym>
    }
    
    token chords {
        <chord>+ ';' <flags>?
    }
    
    token hotkey {
        <chords> ':' <command>
    }
}
