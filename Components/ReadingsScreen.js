import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity, ScrollView } from 'react-native';
import { MaterialCommunityIcons } from '@expo/vector-icons';

export default function ReadingsScreen() {
  const recentReadings = [
    { id: 1, time: '08:50:40 PM', humidity: '69.2%', temperature: '25.5°C' },
    { id: 2, time: '08:40:40 PM', humidity: '68.5%', temperature: '25.2°C' },
    { id: 3, time: '08:30:40 PM', humidity: '70.1%', temperature: '25.8°C' },
  ];

  const pingSummary = [
    { id: 1, device: 'Casing 1', status: 'Online', lastPing: '08:50:40 PM' },
    { id: 2, device: 'Casing 2', status: 'Online', lastPing: '08:45:20 PM' },
    { id: 3, device: 'Casing 3', status: 'Offline', lastPing: '08:30:10 PM' },
  ];

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.header}>Readings & Snapshot</Text>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Recent Readings</Text>
        {recentReadings.map((reading) => (
          <TouchableOpacity key={reading.id} style={styles.readingRow}>
            <Text style={styles.readingTime}>{reading.time}</Text>
            <Text style={styles.readingData}>Humidity: {reading.humidity} | Temp: {reading.temperature}</Text>
          </TouchableOpacity>
        ))}
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Ping Summary</Text>
        <Text style={styles.subTitle}>All Device Pings</Text>
        {pingSummary.map((ping) => (
          <TouchableOpacity key={ping.id} style={styles.pingRow}>
            <MaterialCommunityIcons name="router-wireless" size={20} color="#E67E22" />
            <View style={styles.pingInfo}>
              <Text style={styles.pingDevice}>{ping.device}</Text>
              <Text style={styles.pingStatus}>{ping.status} - Last ping: {ping.lastPing}</Text>
            </View>
          </TouchableOpacity>
        ))}
      </View>
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
  section: {
    marginBottom: 24,
  },
  sectionTitle: {
    fontSize: 20,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
    marginBottom: 12,
  },
  subTitle: {
    fontSize: 16,
    color: '#A0522D',
    fontFamily: 'System',
    marginBottom: 8,
  },
  readingRow: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 12,
    marginBottom: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  readingTime: {
    fontSize: 14,
    color: '#A0522D',
    fontFamily: 'System',
    fontWeight: 'bold',
  },
  readingData: {
    fontSize: 14,
    color: '#A0522D',
    fontFamily: 'System',
  },
  pingRow: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 12,
    marginBottom: 8,
    flexDirection: 'row',
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  pingInfo: {
    marginLeft: 12,
  },
  pingDevice: {
    fontSize: 16,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
  },
  pingStatus: {
    fontSize: 14,
    color: '#A0522D',
    fontFamily: 'System',
  },
});
