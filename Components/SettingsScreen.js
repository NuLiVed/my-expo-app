import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity, ScrollView } from 'react-native';
import { MaterialCommunityIcons } from '@expo/vector-icons';

export default function SettingsScreen() {
  const settings = [
    { id: 1, icon: 'account', label: 'Account Settings' },
    { id: 2, icon: 'bell', label: 'Notifications' },
    { id: 3, icon: 'shield', label: 'Privacy & Security' },
    { id: 4, icon: 'help-circle', label: 'Help & Support' },
    { id: 5, icon: 'information', label: 'About' },
  ];

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.header}>Settings</Text>

      {settings.map((setting) => (
        <TouchableOpacity key={setting.id} style={styles.settingItem}>
          <MaterialCommunityIcons name={setting.icon} size={24} color="#E67E22" />
          <Text style={styles.settingLabel}>{setting.label}</Text>
          <MaterialCommunityIcons name="chevron-right" size={24} color="#A0522D" />
        </TouchableOpacity>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#F5F5F5',
    paddingHorizontal: 16,
  },
  header: {
    fontSize: 28,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
    textAlign: 'center',
    marginTop: 20,
    marginBottom: 16,
  },
  settingItem: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 16,
    marginBottom: 8,
    flexDirection: 'row',
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  settingLabel: {
    fontSize: 16,
    color: '#A0522D',
    fontFamily: 'System',
    marginLeft: 12,
    flex: 1,
  },
});
