import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity, ScrollView } from 'react-native';
import { MaterialCommunityIcons } from '@expo/vector-icons';
import ButtonPrimary from './ButtonPrimary';

export default function DeviceManagerScreen() {
  const devices = [
    { id: 1, name: 'Casing 1', active: true },
    { id: 2, name: 'Casing 2', active: false },
    { id: 3, name: 'Casing 3', active: false },
  ];

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.header}>Device Manager</Text>

      <TouchableOpacity style={styles.addButton}>
        <MaterialCommunityIcons name="plus" size={24} color="#FFFFFF" />
        <Text style={styles.addText}>Add device</Text>
      </TouchableOpacity>

      {devices.map((device) => (
        <TouchableOpacity key={device.id} style={styles.deviceCard}>
          <View style={styles.deviceHeader}>
            <MaterialCommunityIcons name="router-wireless" size={24} color="#E67E22" />
            <Text style={styles.deviceName}>{device.name}</Text>
          </View>
          <View style={styles.deviceActions}>
            <TouchableOpacity style={styles.actionButton}>
              <Text style={styles.actionText}>Set as active</Text>
            </TouchableOpacity>
            <TouchableOpacity style={[styles.actionButton, device.active && styles.activeButton]}>
              <Text style={[styles.actionText, device.active && styles.activeText]}>
                {device.active ? 'Active on dashboard' : 'Set as active'}
              </Text>
            </TouchableOpacity>
          </View>
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
  addButton: {
    backgroundColor: '#E67E22',
    borderRadius: 20,
    paddingVertical: 12,
    paddingHorizontal: 16,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    marginBottom: 16,
  },
  addText: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: 'bold',
    fontFamily: 'System',
    marginLeft: 8,
  },
  deviceCard: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 16,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  deviceHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 12,
  },
  deviceName: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
    marginLeft: 12,
  },
  deviceActions: {
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  actionButton: {
    backgroundColor: '#E67E22',
    borderRadius: 10,
    paddingVertical: 8,
    paddingHorizontal: 12,
    flex: 1,
    marginHorizontal: 4,
    alignItems: 'center',
  },
  activeButton: {
    backgroundColor: '#FFFFFF',
    borderWidth: 1,
    borderColor: '#E67E22',
  },
  actionText: {
    color: '#FFFFFF',
    fontSize: 14,
    fontFamily: 'System',
  },
  activeText: {
    color: '#E67E22',
  },
});
