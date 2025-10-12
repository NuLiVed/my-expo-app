import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity, ScrollView } from 'react-native';
import { MaterialCommunityIcons } from '@expo/vector-icons';

export default function AlertsScreen() {
  const alerts = [
    { id: 1, message: 'Low humidity detected in Casing 1', time: '08:50:40 PM', type: 'warning' },
    { id: 2, message: 'Temperature spike in Casing 2', time: '08:40:20 PM', type: 'error' },
    { id: 3, message: 'Device offline: Casing 3', time: '08:30:10 PM', type: 'info' },
  ];

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.header}>Alerts</Text>

      <View style={styles.alertsList}>
        {alerts.map((alert) => (
          <TouchableOpacity key={alert.id} style={styles.alertItem}>
            <MaterialCommunityIcons
              name={alert.type === 'warning' ? 'alert' : alert.type === 'error' ? 'alert-circle' : 'information'}
              size={24}
              color={alert.type === 'warning' ? '#FF9800' : alert.type === 'error' ? '#F44336' : '#2196F3'}
            />
            <View style={styles.alertInfo}>
              <Text style={styles.alertMessage}>{alert.message}</Text>
              <Text style={styles.alertTime}>{alert.time}</Text>
            </View>
          </TouchableOpacity>
        ))}
      </View>

      <TouchableOpacity style={styles.viewAllButton}>
        <Text style={styles.viewAllText}>View All Alerts</Text>
      </TouchableOpacity>
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
  alertsList: {
    marginBottom: 24,
  },
  alertItem: {
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
  alertInfo: {
    marginLeft: 12,
    flex: 1,
  },
  alertMessage: {
    fontSize: 16,
    color: '#A0522D',
    fontFamily: 'System',
  },
  alertTime: {
    fontSize: 12,
    color: '#999',
    fontFamily: 'System',
  },
  viewAllButton: {
    backgroundColor: '#E67E22',
    borderRadius: 20,
    paddingVertical: 12,
    paddingHorizontal: 24,
    alignItems: 'center',
    marginBottom: 20,
  },
  viewAllText: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: 'bold',
    fontFamily: 'System',
  },
});
