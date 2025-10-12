import React from 'react';
import { Pressable, Text, StyleSheet } from 'react-native';

export default function ButtonPrimary({ label, onPress }) {
  return (
    <Pressable style={({ pressed }) => [styles.button, pressed && styles.pressed]} onPress={onPress}>
      <Text style={styles.text}>{label}</Text>
    </Pressable>
  );
}

const styles = StyleSheet.create({
  button: {
    backgroundColor: '#E67E22',
    borderRadius: 20,
    paddingVertical: 12,
    alignItems: 'center',
    marginBottom: 10,
  },
  pressed: {
    opacity: 0.8,
  },
  text: {
    color: 'white',
    fontWeight: 'bold',
    fontSize: 18,
    fontFamily: 'System',
  },
});
