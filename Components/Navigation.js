import React from 'react';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { NavigationContainer } from '@react-navigation/native';
import { MaterialCommunityIcons } from '@expo/vector-icons';
import DashboardScreen from './DashboardScreen';
import DeviceManagerScreen from './DeviceManagerScreen';
import ReadingsScreen from './ReadingsScreen';
import AlertsScreen from './AlertsScreen';
import LogsScreen from './LogsScreen';
import SettingsScreen from './SettingsScreen';

const Tab = createBottomTabNavigator();

export default function Navigation({ onLogout }) {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={({ route }) => ({
          tabBarIcon: ({ focused, color, size }) => {
            let iconName;

            if (route.name === 'Dashboard') {
              iconName = 'home';
            } else if (route.name === 'Devices') {
              iconName = 'router-wireless';
            } else if (route.name === 'Monitoring') {
              iconName = 'monitor';
            } else if (route.name === 'Alerts') {
              iconName = 'bell';
            } else if (route.name === 'Logs') {
              iconName = 'history';
            } else if (route.name === 'Settings') {
              iconName = 'cog';
            }

            return <MaterialCommunityIcons name={iconName} size={size} color={color} />;
          },
          tabBarActiveTintColor: '#E67E22',
          tabBarInactiveTintColor: '#A0522D',
          headerStyle: { backgroundColor: '#E67E22' },
          headerTintColor: '#FFFFFF',
          headerTitleStyle: { fontWeight: 'bold' },
        })}
      >
        <Tab.Screen name="Dashboard" component={DashboardScreen} />
        <Tab.Screen name="Devices" component={DeviceManagerScreen} />
        <Tab.Screen name="Monitoring" component={ReadingsScreen} />
        <Tab.Screen name="Alerts" component={AlertsScreen} />
        <Tab.Screen name="Logs" component={LogsScreen} />
        <Tab.Screen name="Settings" component={SettingsScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

const styles = StyleSheet.create({
  drawer: {
    flex: 1,
    backgroundColor: '#E67E22',
    paddingTop: 50,
    paddingHorizontal: 20,
  },
  welcomeCard: {
    backgroundColor: 'rgba(255, 255, 255, 0.2)',
    borderRadius: 10,
    padding: 16,
    marginBottom: 20,
  },
  welcomeText: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: 'bold',
    fontFamily: 'System',
  },
  navItem: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 12,
    paddingHorizontal: 16,
    marginBottom: 10,
    borderRadius: 20,
  },
  navText: {
    color: '#FFFFFF',
    fontSize: 16,
    marginLeft: 16,
    fontFamily: 'System',
  },
});
