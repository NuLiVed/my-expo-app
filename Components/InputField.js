import React, { useState } from 'react';
import { View, TextInput, StyleSheet } from 'react-native';

export default function InputField({ placeholder, value, onChangeText, secureTextEntry }) {
  const [isFocused, setIsFocused] = useState(false);

  return (
    <View style={styles.container}>
      <TextInput
        style={[styles.input, isFocused && styles.inputFocused]}
        placeholder={placeholder}
        placeholderTextColor="#b8a17f"
        value={value}
        onChangeText={onChangeText}
        secureTextEntry={secureTextEntry}
        multiline={false}
        numberOfLines={1}
        onFocus={() => setIsFocused(true)}
        onBlur={() => setIsFocused(false)}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    width: '100%',
    marginBottom: 14,
  },
  input: {
    backgroundColor: '#fff',
    borderRadius: 12,
    borderWidth: 1,
    borderColor: '#d4bfa3',
    paddingHorizontal: 14,
    height: 48,
    fontSize: 16,
    color: '#4e3b1f',
  },
  inputFocused: {
    borderColor: '#E67E22',
  },
});
